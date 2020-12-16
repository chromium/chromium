// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_test_util.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace chromeos {
namespace test {

const char kUserAvatarImage1RelativePath[] = "chromeos/avatar1.jpg";
const char kUserAvatarImage2RelativePath[] = "chromeos/avatar2.jpg";
const char kUserAvatarImage3RelativePath[] = "chromeos/avatar3.png";

bool AreImagesEqual(const gfx::ImageSkia& first, const gfx::ImageSkia& second) {
  if (first.width() != second.width() || first.height() != second.height())
    return false;
  const SkBitmap* first_bitmap = first.bitmap();
  const SkBitmap* second_bitmap = second.bitmap();
  if (!first_bitmap && !second_bitmap)
    return true;
  if (!first_bitmap || !second_bitmap)
    return false;

  const size_t size = first_bitmap->computeByteSize();
  if (second_bitmap->computeByteSize() != size)
    return false;

  uint8_t* first_data = reinterpret_cast<uint8_t*>(first_bitmap->getPixels());
  uint8_t* second_data = reinterpret_cast<uint8_t*>(second_bitmap->getPixels());
  for (size_t i = 0; i < size; ++i) {
    if (first_data[i] != second_data[i])
      return false;
  }
  return true;
}

ImageLoader::ImageLoader(const base::FilePath& path) : path_(path) {}

ImageLoader::~ImageLoader() {}

gfx::ImageSkia ImageLoader::Load() {
  std::string image_data;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ReadFileToString(path_, &image_data);
  }
  const ImageDecoder::ImageCodec codec =
      (path_.Extension() == FILE_PATH_LITERAL(".jpg")
       ? ImageDecoder::DEFAULT_CODEC
       : ImageDecoder::ROBUST_PNG_CODEC);
  ImageDecoder::StartWithOptions(this, image_data, codec, false);
  run_loop_.Run();
  return decoded_image_;
}

void ImageLoader::OnImageDecoded(const SkBitmap& decoded_image) {
  decoded_image_ = gfx::ImageSkia(gfx::ImageSkiaRep(decoded_image, 1.0f));
  run_loop_.Quit();
}

void ImageLoader::OnDecodeImageFailed() {
  decoded_image_ = gfx::ImageSkia();
  run_loop_.Quit();
}

}  // namespace test
}  // namespace chromeos
