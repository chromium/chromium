// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_loader_delegate_impl.h"

#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"

namespace ash {

UserImageLoaderDelegateImpl::UserImageLoaderDelegateImpl() = default;

UserImageLoaderDelegateImpl::~UserImageLoaderDelegateImpl() = default;

void UserImageLoaderDelegateImpl::FromGURLAnimated(
    const GURL& default_image_url,
    user_image_loader::LoadedCallback loaded_cb) {
  user_image_loader::StartWithGURLAnimated(default_image_url,
                                           std::move(loaded_cb));
}

}  // namespace ash
