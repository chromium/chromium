// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_MOCK_USER_IMAGE_LOADER_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_MOCK_USER_IMAGE_LOADER_DELEGATE_H_

#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"
#include "chrome/browser/ash/login/users/avatar/user_image_loader_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace ash::test {

class MockUserImageLoaderDelegate : public UserImageLoaderDelegate {
 public:
  MockUserImageLoaderDelegate();

  MockUserImageLoaderDelegate(const MockUserImageLoaderDelegate&) = delete;
  MockUserImageLoaderDelegate& operator=(const MockUserImageLoaderDelegate&) =
      delete;

  ~MockUserImageLoaderDelegate() override;

  MOCK_METHOD(void,
              FromGURLAnimated,
              (const GURL& default_image_url,
               user_image_loader::LoadedCallback loaded_cb),
              (override));
};

}  // namespace ash::test

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_MOCK_USER_IMAGE_LOADER_DELEGATE_H_
