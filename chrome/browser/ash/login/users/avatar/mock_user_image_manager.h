// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_MOCK_USER_IMAGE_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_MOCK_USER_IMAGE_MANAGER_H_

#include "base/files/file_path.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "components/user_manager/user_image/user_image.h"
#include "testing/gmock/include/gmock/gmock.h"

class AccountId;

namespace ash {

class MockUserImageManager : public UserImageManager {
 public:
  explicit MockUserImageManager(const AccountId& account_id);
  ~MockUserImageManager() override;

  MOCK_METHOD(void, LoadUserImage, (), (override));
  MOCK_METHOD(void,
              UserLoggedIn,
              (bool user_is_new, bool user_is_local),
              (override));
  MOCK_METHOD(void, UserProfileCreated, (), (override));
  MOCK_METHOD(void, SaveUserDefaultImageIndex, (int image_index), (override));
  MOCK_METHOD(void,
              SaveUserImage,
              (std::unique_ptr<user_manager::UserImage> user_image),
              (override));
  MOCK_METHOD(void,
              SaveUserImageFromFile,
              (const base::FilePath& path),
              (override));
  MOCK_METHOD(void, SaveUserImageFromProfileImage, (), (override));
  MOCK_METHOD(void, DeleteUserImage, (), (override));
  MOCK_METHOD(void, DownloadProfileImage, (), (override));
  MOCK_METHOD(const gfx::ImageSkia&,
              DownloadedProfileImage,
              (),
              (const, override));
  MOCK_METHOD(UserImageSyncObserver*, GetSyncObserver, (), (const, override));
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(bool, IsUserImageManaged, (), (const, override));
  MOCK_METHOD(void, OnExternalDataSet, (const std::string& policy), (override));
  MOCK_METHOD(void,
              OnExternalDataCleared,
              (const std::string& policy),
              (override));
  MOCK_METHOD(void,
              OnExternalDataFetched,
              (const std::string& policy, std::unique_ptr<std::string> data),
              (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_MOCK_USER_IMAGE_MANAGER_H_
