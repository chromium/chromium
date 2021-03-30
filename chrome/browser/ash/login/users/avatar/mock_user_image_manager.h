// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_MOCK_USER_IMAGE_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_MOCK_USER_IMAGE_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "components/user_manager/user_image/user_image.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockUserImageManager : public UserImageManager {
 public:
  explicit MockUserImageManager(const std::string& user_id);
  virtual ~MockUserImageManager();

  MOCK_METHOD1(SaveUserDefaultImageIndex, void(int));
  void SaveUserImage(std::unique_ptr<user_manager::UserImage>) {}
  MOCK_METHOD1(SaveUserImageFromFile, void(const base::FilePath&));
  MOCK_METHOD0(SaveUserImageFromProfileImage, void());
  MOCK_METHOD0(DownloadProfileImage, void());
  MOCK_CONST_METHOD0(DownloadedProfileImage, const gfx::ImageSkia&(void));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_MOCK_USER_IMAGE_MANAGER_H_
