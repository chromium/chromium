// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_DECODER_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_DECODER_H_

#include <string>

#include "base/callback_forward.h"

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class ImageSkia;
}

namespace ash {

using DecodeImageCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

void DecodeImageFile(DecodeImageCallback callback,
                     const base::FilePath& file_path);

void DecodeImageData(DecodeImageCallback callback, const std::string& data);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_DECODER_H_
