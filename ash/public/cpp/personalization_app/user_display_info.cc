// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/personalization_app/user_display_info.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user.h"

namespace ash {
namespace personalization_app {

UserDisplayInfo::UserDisplayInfo() = default;

UserDisplayInfo::UserDisplayInfo(const user_manager::User& user)
    : email(user.GetDisplayEmail()),
      name(base::UTF16ToUTF8(user.GetDisplayName())) {}

UserDisplayInfo::UserDisplayInfo(UserDisplayInfo&&) = default;
UserDisplayInfo& UserDisplayInfo::operator=(UserDisplayInfo&&) = default;

UserDisplayInfo::~UserDisplayInfo() = default;

}  // namespace personalization_app
}  // namespace ash
