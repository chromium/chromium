// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SESSION_USER_INFO_H_
#define ASH_PUBLIC_CPP_SESSION_USER_INFO_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/token.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Data for a user's avatar.
struct ASH_PUBLIC_EXPORT UserAvatar {
  UserAvatar();
  UserAvatar(const UserAvatar& other);
  ~UserAvatar();

  gfx::ImageSkia image;
  // The raw bytes for the avatar. Useful if the avatar is animated.
  std::vector<uint8_t> bytes;
};

ASH_PUBLIC_EXPORT bool operator==(const UserAvatar& a, const UserAvatar& b);

// Info about a user.
struct ASH_PUBLIC_EXPORT UserInfo {
  UserInfo();
  UserInfo(const UserInfo& other);
  ~UserInfo();

  user_manager::UserType type = user_manager::UserType::kRegular;
  AccountId account_id;
  std::string display_name;
  std::string display_email;
  std::string given_name;
  UserAvatar avatar;

  // True if this user has a newly created profile (first time login on the
  // device)
  bool is_new_profile = false;

  // True if the user's non-cryptohome data (wallpaper, avatar etc.) is
  // ephemeral. See |UserManager::IsUserNonCryptohomeDataEphemeral| for details.
  bool is_ephemeral = false;

  // True if the user has a gaia account.
  bool has_gaia_account = false;

  // True if should display managed ui.
  bool should_display_managed_ui = false;

  // True if the account specified by `account_id` is under policy management.
  bool is_managed = false;
};

ASH_PUBLIC_EXPORT bool operator==(const UserInfo& a, const UserInfo& b);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SESSION_USER_INFO_H_
