// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_default_device_name.h"

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/devicetype.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

base::Optional<base::string16> GetNameFromProfile(Profile* profile) {
  if (!profile)
    return base::nullopt;

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return base::nullopt;

  base::string16 name = user->GetGivenName();
  return name.empty() ? base::nullopt : base::make_optional(name);
}

}  // namespace

std::string GetNearbyShareDefaultDeviceName(Profile* profile) {
  base::string16 device_type = ui::GetChromeOSDeviceName();
  base::Optional<base::string16> name_from_profile =
      GetNameFromProfile(profile);
  if (!name_from_profile)
    return base::UTF16ToUTF8(device_type);

  return l10n_util::GetStringFUTF8(IDS_NEARBY_DEFAULT_DEVICE_NAME,
                                   *name_from_profile, device_type);
}
