// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IMAGE_UTIL_H_
#define ASH_PUBLIC_CPP_IMAGE_UTIL_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class ImageSkia;
class Size;
}  // namespace gfx

namespace ash {
namespace image_util {

// Returns a `gfx::ImageSkia` of the specified `size` which draws nothing.
ASH_PUBLIC_EXPORT gfx::ImageSkia CreateEmptyImage(const gfx::Size& size);

using DecodeImageCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

// Reads contents at |file_path| and calls |callback| with a decoded image.
// Calls |callback| with an empty image on failure to read the file or decode
// the image.
// If the image is too large, it will be repeatedly halved until it fits in
// |IPC::Channel::kMaximumMessageSize| bytes.
ASH_PUBLIC_EXPORT void DecodeImageFile(
    DecodeImageCallback callback,
    const base::FilePath& file_path,
    data_decoder::mojom::ImageCodec codec =
        data_decoder::mojom::ImageCodec::kDefault);

// Reads contents of |data| and calls |callback| with a decoded image.
// Calls |callback| with an empty image on failure to decode the image.
// If the image is too large, it will be repeatedly halved until it fits in
// |IPC::Channel::kMaximumMessageSize| bytes.
ASH_PUBLIC_EXPORT void DecodeImageData(DecodeImageCallback callback,
                                       data_decoder::mojom::ImageCodec codec,
                                       const std::string& data);

}  // namespace image_util
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IMAGE_UTIL_H_
