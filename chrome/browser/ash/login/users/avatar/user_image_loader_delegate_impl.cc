// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_loader_delegate_impl.h"

#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

UserImageLoaderDelegateImpl::UserImageLoaderDelegateImpl(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : shared_url_loader_factory_(std::move(shared_url_loader_factory)) {
  if (!shared_url_loader_factory_) {
    CHECK_IS_TEST();
  }
}

UserImageLoaderDelegateImpl::~UserImageLoaderDelegateImpl() = default;

void UserImageLoaderDelegateImpl::FromGURLAnimated(
    const GURL& default_image_url,
    user_image_loader::LoadedCallback loaded_cb) {
  CHECK(shared_url_loader_factory_);
  user_image_loader::StartWithGURLAnimated(shared_url_loader_factory_.get(),
                                           default_image_url,
                                           std::move(loaded_cb));
}

}  // namespace ash
