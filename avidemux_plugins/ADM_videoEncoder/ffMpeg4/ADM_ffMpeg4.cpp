/***************************************************************************
                          \fn ADM_ffMpeg4
                          \brief Front end for libavcodec Mpeg4 asp encoder
                             -------------------

    copyright            : (C) 2002/2009 by mean
    email                : fixounet@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ADM_default.h"
#include "ADM_ffMpeg4.h"
#undef ADM_MINIMAL_UI_INTERFACE // we need the full UI
#include "DIA_factory.h"

#if 1
        #define aprintf(...) {}
#else
        #define aprintf printf
#endif

FFcodecSettings Mp4Settings = MPEG4_CONF_DEFAULT;

/**
        \fn ADM_ffMpeg4Encoder
*/
ADM_ffMpeg4Encoder::ADM_ffMpeg4Encoder(ADM_coreVideoFilter *src,bool globalHeader) : ADM_coreVideoEncoderFFmpeg(src,&Mp4Settings,globalHeader)
{
    printf("[ffMpeg4Encoder] Creating.\n");


}

/**
    \fn pre-open
*/
bool ADM_ffMpeg4Encoder::configureContext(void)
{

    switch(Settings.params.mode)
    {
      case COMPRESS_2PASS:
      case COMPRESS_2PASS_BITRATE:
           if(false==setupPass())
            {
                printf("[ffmpeg] Multipass setup failed\n");
                return false;
            }
            break;
      case COMPRESS_SAME:
      case COMPRESS_CQ:
            _context->flags |= AV_CODEC_FLAG_QSCALE;
            _context->bit_rate = 0;
            break;
      case COMPRESS_CBR:
              _context->bit_rate=Settings.params.bitrate*1000; // kb->b;
            break;
     default:
            return false;
    }
    presetContext(&Settings);
    
    return true;
}

/**
    \fn setup
*/
bool ADM_ffMpeg4Encoder::setup(void)
{

    if(false== ADM_coreVideoEncoderFFmpeg::setup(AV_CODEC_ID_MPEG4))
        return false;

    printf("[ffMpeg] Setup ok\n");
    return true;
}


/**
    \fn ~ADM_ffMpeg4Encoder
*/
ADM_ffMpeg4Encoder::~ADM_ffMpeg4Encoder()
{
    printf("[ffMpeg4Encoder] Destroying.\n");


}

/**
    \fn encode
*/
bool         ADM_ffMpeg4Encoder::encode (ADMBitstream * out)
{
int sz,q,r;
again:
    sz=0;
    if(false==preEncode()) // Pop - out the frames stored in the queue due to B-frames
    {
        r=encodeWrapper(NULL,out);

        if(encoderState == ADM_ENCODER_STATE_FLUSHED)
        {
            ADM_info("[ffMpeg4] End of stream.\n");
            return false;
        }
        if(r<0)
        {
            ADM_warning("[ffMpeg4] Error %d encoding video\n",r);
            return false;
        }
        sz=r;
        if(!sz) return false;
        printf("[ffmpeg4] Popping delayed bframes (%d)\n",sz);
        goto link;
    }
    q=image->_Qp;

    if(!q) q=2;
    switch(Settings.params.mode)
    {
      case COMPRESS_SAME:
                // Keep same frame type & same Qz as the incoming frame...
            _frame->quality = (int) floor (FF_QP2LAMBDA * q+ 0.5);

            if(image->flags & AVI_KEY_FRAME)    _frame->pict_type = AV_PICTURE_TYPE_I;
            else if(image->flags & AVI_B_FRAME) _frame->pict_type = AV_PICTURE_TYPE_B;
            else                                _frame->pict_type = AV_PICTURE_TYPE_P;

            break;
      case COMPRESS_2PASS:
      case COMPRESS_2PASS_BITRATE:
            switch(pass)
            {
                case 1:
                        break;
                case 2:
                        break; // Get Qz for this frame...
            }
      case COMPRESS_CQ:
            _frame->quality = (int) floor (FF_QP2LAMBDA * Settings.params.qz+ 0.5);
            break;
      case COMPRESS_CBR:
            break;
     default:
            printf("[ffMpeg4] Unsupported encoding mode\n");
            return false;
    }
    aprintf("[CODEC] Flags = 0x%x, QSCALE=%x, bit_rate=%d, quality=%d qz=%d incoming qz=%d\n",_context->flags,CODEC_FLAG_QSCALE,
                                     _context->bit_rate,  _frame->quality, _frame->quality/ FF_QP2LAMBDA,q);

    _frame->reordered_opaque=image->Pts;
    r=encodeWrapper(_frame,out);
    if(encoderState == ADM_ENCODER_STATE_FLUSHED)
    {
        ADM_info("[ffMpeg4] End of stream.\n");
        return false;
    }
    if(r<0)
    {
        ADM_warning("[ffMpeg4] Error %d encoding video\n",r);
        return false;
    }
    sz=r;
    if(sz==0) // no pic, probably pre filling, try again
        goto again;
link:
    return postEncode(out,sz);
}

/**
    \fn isDualPass

*/
bool         ADM_ffMpeg4Encoder::isDualPass(void)
{
    if(Settings.params.mode==COMPRESS_2PASS || Settings.params.mode==COMPRESS_2PASS_BITRATE ) return true;
    return false;

}

/**
    \fn jpegConfigure
    \brief UI configuration for jpeg encoder
*/

bool         ffMpeg4Configure(void)
{
diaMenuEntry qzE[]={
  {0,QT_TRANSLATE_NOOP("ffmpeg4","H.263"),NULL},
  {1,QT_TRANSLATE_NOOP("ffmpeg4","MPEG"),NULL}
};

diaMenuEntry rdE[]={
  {0,QT_TRANSLATE_NOOP("ffmpeg4","MB comparison"),NULL},
  {1,QT_TRANSLATE_NOOP("ffmpeg4","Fewest bits (vhq)"),NULL},
  {2,QT_TRANSLATE_NOOP("ffmpeg4","Rate distortion"),NULL}
};
diaMenuEntry threads[]={
  {0,QT_TRANSLATE_NOOP("ffmpeg4","One thread"),NULL},
  {2,QT_TRANSLATE_NOOP("ffmpeg4","Two threads"),NULL},
  {3,QT_TRANSLATE_NOOP("ffmpeg4","Three threads"),NULL},
  {99,QT_TRANSLATE_NOOP("ffmpeg4","Auto (#cpu)"),NULL}
};


        FFcodecSettings *conf=&Mp4Settings;

#define PX(x) &(conf->lavcSettings.x)

         diaElemBitrate   bitrate(&(Mp4Settings.params),NULL);
         diaElemMenu      threadM(PX(MultiThreaded),QT_TRANSLATE_NOOP("ffmpeg4","Threading"),4,threads);
         diaElemUInteger  qminM(PX(qmin),QT_TRANSLATE_NOOP("ffmpeg4","Mi_n. quantizer:"),0,31);
         diaElemUInteger  qmaxM(PX(qmax),QT_TRANSLATE_NOOP("ffmpeg4","Ma_x. quantizer:"),0,31);
         diaElemUInteger  qdiffM(PX(max_qdiff),QT_TRANSLATE_NOOP("ffmpeg4","Max. quantizer _difference:"),0,31);

         diaElemToggle    fourMv(PX(_4MV),QT_TRANSLATE_NOOP("ffmpeg4","4_MV"));
         diaElemToggle    trellis(PX(_TRELLIS_QUANT),QT_TRANSLATE_NOOP("ffmpeg4","_Trellis quantization"));

         diaElemToggle    qpel(PX(_QPEL),QT_TRANSLATE_NOOP("ffmpeg4","_Quarter pixel"));

         diaElemUInteger  max_b_frames(PX(max_b_frames),QT_TRANSLATE_NOOP("ffmpeg4","_Number of B frames:"),0,32);
         diaElemMenu     qzM(PX(mpeg_quant),QT_TRANSLATE_NOOP("ffmpeg4","_Quantization type:"),2,qzE);

         diaElemMenu     rdM(PX(mb_eval),QT_TRANSLATE_NOOP("ffmpeg4","_Macroblock decision:"),3,rdE);

         diaElemUInteger filetol(PX(vratetol),QT_TRANSLATE_NOOP("ffmpeg4","_Filesize tolerance (kb):"),0,100000);

         diaElemFloat    qzComp(PX(qcompress),QT_TRANSLATE_NOOP("ffmpeg4","_Quantizer compression:"),0,1);
         diaElemFloat    qzBlur(PX(qblur),QT_TRANSLATE_NOOP("ffmpeg4","Quantizer _blur:"),0,1);

        diaElemUInteger GopSize(PX(gop_size),QT_TRANSLATE_NOOP("ffmpeg4","_Gop Size:"),1,500);
          /* First Tab : encoding mode */
        diaElem *diamode[]={&GopSize,&threadM,&bitrate};
        diaElemTabs tabMode(QT_TRANSLATE_NOOP("ffmpeg4","User Interface"),3,diamode);

        /* 2nd Tab : ME */
        diaElemFrame frameMe(QT_TRANSLATE_NOOP("ffmpeg4","Advanced Simple Profile"));

        frameMe.swallow(&max_b_frames);
        frameMe.swallow(&qpel);

        diaElem *diaME[]={&fourMv,&frameMe};
        diaElemTabs tabME(QT_TRANSLATE_NOOP("ffmpeg4","Motion Estimation"),2,diaME);
        /* 3nd Tab : Qz */

         diaElem *diaQze[]={&qzM,&rdM,&qminM,&qmaxM,&qdiffM,&trellis};
        diaElemTabs tabQz(QT_TRANSLATE_NOOP("ffmpeg4","Quantization"),6,diaQze);

        /* 4th Tab : RControl */

         diaElem *diaRC[]={&filetol,&qzComp,&qzBlur};
        diaElemTabs tabRC(QT_TRANSLATE_NOOP("ffmpeg4","Rate Control"),3,diaRC);

         diaElemTabs *tabs[]={&tabMode,&tabME,&tabQz,&tabRC};
        if( diaFactoryRunTabs(QT_TRANSLATE_NOOP("ffmpeg4","libavcodec MPEG-4 configuration"),4,tabs))
        {
          return true;
        }
         return false;
}
// EOF
