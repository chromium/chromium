// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"

namespace ash {

UserImageManager::UserImageManager(const AccountId& account_id)
    : account_id_(account_id) {}

UserImageManager::~UserImageManager() {}

}  // namespace ash
