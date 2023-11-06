// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IMAGE_UTIL_H_
#define ASH_PUBLIC_CPP_IMAGE_UTIL_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace gfx {
class ImageSkia;
class Size;
}  // namespace gfx

namespace ash {
namespace image_util {

// Returns a `gfx::ImageSkia` of the specified `size` which draws nothing.
ASH_PUBLIC_EXPORT gfx::ImageSkia CreateEmptyImage(const gfx::Size& size);

// Resizes a `gfx::ImageSkia` to just fill the entirety of `new_size`,
// maintaining aspect ratio, and crops any portion of the image outside of the
// bounds of `new_size`.
ASH_PUBLIC_EXPORT gfx::ImageSkia ResizeAndCropImage(
    const gfx::ImageSkia& image_skia,
    const gfx::Size& new_size);

struct ASH_PUBLIC_EXPORT AnimationFrame {
  gfx::ImageSkia image;
  base::TimeDelta duration;
};

using DecodeImageCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;
using DecodeAnimationCallback =
    base::OnceCallback<void(std::vector<AnimationFrame>)>;

// TESTING NOTE: See ash::InProcessImageDecoder for unit testing code that
// calls any of the DecodeImage*() functions below.

// Reads contents at `file_path` and calls `callback` with a decoded image or
// animation, respectively.
// Calls `callback` with an empty image/vector on
// failure to read the file or decode the image.
// If the image is too large, it will be repeatedly halved until it fits in
// `IPC::Channel::kMaximumMessageSize` bytes.
//
// A custom `file_task_runner` may be specified if desired; if not, an arbitrary
// task runner is used internally.
ASH_PUBLIC_EXPORT void DecodeImageFile(
    DecodeImageCallback callback,
    const base::FilePath& file_path,
    data_decoder::mojom::ImageCodec codec =
        data_decoder::mojom::ImageCodec::kDefault,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner = nullptr);
ASH_PUBLIC_EXPORT void DecodeAnimationFile(
    DecodeAnimationCallback callback,
    const base::FilePath& file_path,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner = nullptr);

// Reads contents of `data` and calls `callback` with a decoded image or
// animation, respectively.
// Calls `callback` with an empty image/vector on failure to decode the
// image.
// If the image is too large, it will be repeatedly halved until it fits in
// `IPC::Channel::kMaximumMessageSize` bytes.
ASH_PUBLIC_EXPORT void DecodeImageData(DecodeImageCallback callback,
                                       data_decoder::mojom::ImageCodec codec,
                                       const std::string& data);
ASH_PUBLIC_EXPORT void DecodeAnimationData(DecodeAnimationCallback callback,
                                           const std::string& data);

}  // namespace image_util
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IMAGE_UTIL_H_
