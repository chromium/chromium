// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_DELEGATE_IMPL_H_

#include "chrome/browser/ash/login/users/avatar/user_image_loader_delegate.h"
namespace ash {

// Concrete implementation. Makes real network requests to download avatar
// images.
class UserImageLoaderDelegateImpl : public UserImageLoaderDelegate {
 public:
  UserImageLoaderDelegateImpl();

  UserImageLoaderDelegateImpl(const UserImageLoaderDelegateImpl&) = delete;
  UserImageLoaderDelegateImpl& operator=(const UserImageLoaderDelegateImpl&) =
      delete;

  ~UserImageLoaderDelegateImpl() override;

  // UserImageLoaderDelegate:
  void FromGURLAnimated(const GURL& default_image_url,
                        user_image_loader::LoadedCallback loaded_cb) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_DELEGATE_IMPL_H_
