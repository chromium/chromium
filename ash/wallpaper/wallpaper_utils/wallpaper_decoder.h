// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_DECODER_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_DECODER_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "services/data_decoder/public/cpp/decode_image.h"

namespace gfx {
class ImageSkia;
}

namespace ash {

using OnWallpaperDecoded =
    base::OnceCallback<void(const gfx::ImageSkia& image)>;

// Do an async wallpaper decode; |on_decoded| is run on the calling thread when
// the decode has finished.
void DecodeWallpaper(const std::string& image_data,
                     const data_decoder::mojom::ImageCodec& image_codec,
                     OnWallpaperDecoded callback);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_DECODER_H_
