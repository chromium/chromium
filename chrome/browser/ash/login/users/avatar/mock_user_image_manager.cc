// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/mock_user_image_manager.h"

namespace ash {

MockUserImageManager::MockUserImageManager(const AccountId& account_id)
    : UserImageManager(account_id) {}

MockUserImageManager::~MockUserImageManager() = default;

}  // namespace ash
