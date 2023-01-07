// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/session/user_info.h"

namespace ash {

UserAvatar::UserAvatar() = default;
UserAvatar::UserAvatar(const UserAvatar& other) = default;
UserAvatar::~UserAvatar() = default;

bool operator==(const UserAvatar& a, const UserAvatar& b) {
  return a.image.BackedBySameObjectAs(b.image) && a.bytes == b.bytes;
}

UserInfo::UserInfo() = default;
UserInfo::UserInfo(const UserInfo& other) = default;
UserInfo::~UserInfo() = default;

ASH_PUBLIC_EXPORT bool operator==(const UserInfo& a, const UserInfo& b) {
  return a.type == b.type && a.account_id == b.account_id &&
         a.display_name == b.display_name &&
         a.display_email == b.display_email && a.given_name == b.given_name &&
         a.avatar == b.avatar && a.is_new_profile == b.is_new_profile &&
         a.is_ephemeral == b.is_ephemeral &&
         a.has_gaia_account == b.has_gaia_account &&
         a.should_display_managed_ui == b.should_display_managed_ui;
}

}  // namespace ash
