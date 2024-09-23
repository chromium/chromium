// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_TEST_UTIL_H_

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class FilePath;
}

namespace ash::test {

extern const char kUserAvatarImage1RelativePath[];
extern const char kUserAvatarImage2RelativePath[];
// Points to a png file with transparent pixels.
extern const char kUserAvatarImage3RelativePath[];

// Returns `true` if the two given images are pixel-for-pixel identical.
bool AreImagesEqual(const gfx::ImageSkia& first, const gfx::ImageSkia& second);

class ImageLoader : public ImageDecoder::ImageRequest {
 public:
  explicit ImageLoader(const base::FilePath& path);

  ImageLoader(const ImageLoader&) = delete;
  ImageLoader& operator=(const ImageLoader&) = delete;

  ~ImageLoader() override;

  gfx::ImageSkia Load();

  // ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

 private:
  base::FilePath path_;
  base::RunLoop run_loop_;

  gfx::ImageSkia decoded_image_;
};

class UserImageChangeWaiter : public user_manager::UserManager::Observer {
 public:
  UserImageChangeWaiter();

  UserImageChangeWaiter(const UserImageChangeWaiter&) = delete;
  UserImageChangeWaiter& operator=(const UserImageChangeWaiter&) = delete;

  ~UserImageChangeWaiter() override;

  void Wait();

  // user_manager::UserManager::Observer:
  void OnUserImageChanged(const user_manager::User& user) override;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace ash::test

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_TEST_UTIL_H_
