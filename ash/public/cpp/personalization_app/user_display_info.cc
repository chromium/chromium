// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/personalization_app/user_display_info.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::personalization_app {

UserDisplayInfo::UserDisplayInfo() = default;

UserDisplayInfo::UserDisplayInfo(const user_manager::User& user)
    : email(user.GetDisplayEmail()) {
  if (user.GetAccountId() == user_manager::GuestAccountId()) {
    name = base::UTF16ToUTF8(
        l10n_util::GetStringUTF16(IDS_PERSONALIZATION_APP_GUEST_NAME));
  } else {
    name = base::UTF16ToUTF8(user.GetDisplayName());
  }
}

UserDisplayInfo::UserDisplayInfo(UserDisplayInfo&&) = default;
UserDisplayInfo& UserDisplayInfo::operator=(UserDisplayInfo&&) = default;

UserDisplayInfo::~UserDisplayInfo() = default;

}  // namespace ash::personalization_app
