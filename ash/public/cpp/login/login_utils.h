// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_LOGIN_UTILS_H_
#define ASH_PUBLIC_CPP_LOGIN_LOGIN_UTILS_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/session/user_info.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace ash {

// Builds a `UserAvatar` instance which contains the current image for `user`.
ASH_PUBLIC_EXPORT UserAvatar
BuildAshUserAvatarForUser(const user_manager::User& user);

// Builds a `UserAvatar` instance which contains the current image
// for `account_id`.
ASH_PUBLIC_EXPORT UserAvatar
BuildAshUserAvatarForAccountId(const AccountId& account_id);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_LOGIN_UTILS_H_
