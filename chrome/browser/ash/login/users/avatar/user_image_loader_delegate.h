// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_DELEGATE_H_

#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"
#include "url/gurl.h"

namespace ash {

// An interface for downloading user avatar images from network. Can be mocked
// in tests.
class UserImageLoaderDelegate {
 public:
  virtual ~UserImageLoaderDelegate() = default;

  virtual void FromGURLAnimated(
      const GURL& default_image_url,
      user_image_loader::LoadedCallback loaded_cb) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_DELEGATE_H_
