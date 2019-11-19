// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_TEST_UTIL_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/image_decoder.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class FilePath;
}

namespace chromeos {
namespace test {

extern const char kUserAvatarImage1RelativePath[];
extern const char kUserAvatarImage2RelativePath[];
// Points to a png file with transparent pixels.
extern const char kUserAvatarImage3RelativePath[];

// Returns |true| if the two given images are pixel-for-pixel identical.
bool AreImagesEqual(const gfx::ImageSkia& first, const gfx::ImageSkia& second);

class ImageLoader : public ImageDecoder::ImageRequest {
 public:
  explicit ImageLoader(const base::FilePath& path);
  ~ImageLoader() override;

  gfx::ImageSkia Load();

  // ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

 private:
  base::FilePath path_;
  base::RunLoop run_loop_;

  gfx::ImageSkia decoded_image_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoader);
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_TEST_UTIL_H_
