/*
 * BMP parser
 * Copyright (c) 2012 Paul B Mahol
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * BMP parser
 */

#include "libavutil/bswap.h"
#include "libavutil/common.h"

#include "parser.h"

typedef struct BMPParseContext {
    ParseContext pc;
    uint32_t fsize;
    uint32_t remaining_size;
} BMPParseContext;

static int bmp_parse(AVCodecParserContext *s, AVCodecContext *avctx,
                     const uint8_t **poutbuf, int *poutbuf_size,
                     const uint8_t *buf, int buf_size)
{
    BMPParseContext *bpc = s->priv_data;
    uint64_t state = bpc->pc.state64;
    int next = END_NOT_FOUND;
    int i = 0;

    *poutbuf_size = 0;

    if (bpc->pc.frame_start_found <= 2+4+4) {
        for (; i < buf_size; i++) {
            state = (state << 8) | buf[i];
            if (bpc->pc.frame_start_found == 0) {
                if ((state >> 48) == (('B' << 8) | 'M')) {
                    bpc->fsize = av_bswap32(state >> 16);
                    bpc->pc.frame_start_found = 1;
                }
            } else if (bpc->pc.frame_start_found == 2+4+4) {
//                 unsigned hsize = av_bswap32(state>>32);
                unsigned ihsize = av_bswap32(state);
                if (ihsize < 12 || ihsize > 200) {
                    bpc->pc.frame_start_found = 0;
                    continue;
                }
                if (bpc->fsize <= ihsize + 14)
                    bpc->fsize = INT_MAX/2;
                bpc->pc.frame_start_found++;
                if (bpc->fsize > buf_size - i + 17)
                    bpc->remaining_size = bpc->fsize - buf_size + i - 17;
                else
                    next = bpc->fsize + i - 17;
                break;
            } else if (bpc->pc.frame_start_found)
                bpc->pc.frame_start_found++;
        }
        bpc->pc.state64 = state;
    } else {
        if (bpc->remaining_size) {
            i = FFMIN(bpc->remaining_size, buf_size);
            bpc->remaining_size -= i;
            if (bpc->remaining_size)
                goto flush;
            next = i;
        }
    }

flush:
    if (ff_combine_frame(&bpc->pc, next, &buf, &buf_size) < 0)
        return buf_size;

    bpc->pc.frame_start_found = 0;

    *poutbuf      = buf;
    *poutbuf_size = buf_size;
    return next;
}

AVCodecParser ff_bmp_parser = {
    .codec_ids      = { AV_CODEC_ID_BMP },
    .priv_data_size = sizeof(BMPParseContext),
    .parser_parse   = bmp_parse,
    .parser_close   = ff_parse_close,
};
