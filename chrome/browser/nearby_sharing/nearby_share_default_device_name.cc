// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_default_device_name.h"

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
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

std::string GetTruncatedName(std::string name, size_t overflow_length) {
  std::string ellipsis("...");
  size_t max_name_length = name.length() - overflow_length - ellipsis.length();
  DCHECK_GT(max_name_length, 0);
  std::string truncated;
  base::TruncateUTF8ToByteSize(name, max_name_length, &truncated);
  truncated.append(ellipsis);
  return truncated;
}

}  // namespace

std::string GetNearbyShareDefaultDeviceName(Profile* profile) {
  base::string16 device_type = ui::GetChromeOSDeviceName();
  base::Optional<base::string16> name_from_profile =
      GetNameFromProfile(profile);
  if (!name_from_profile)
    return base::UTF16ToUTF8(device_type);

  std::string device_name = l10n_util::GetStringFUTF8(
      IDS_NEARBY_DEFAULT_DEVICE_NAME, *name_from_profile, device_type);
  if (device_name.length() > kNearbyShareDeviceNameMaxLength) {
    std::string truncated_name = GetTruncatedName(
        base::UTF16ToUTF8(*name_from_profile),
        device_name.length() - kNearbyShareDeviceNameMaxLength);
    device_name = l10n_util::GetStringFUTF8(IDS_NEARBY_DEFAULT_DEVICE_NAME,
                                            base::UTF8ToUTF16(truncated_name),
                                            device_type);
  }

  return device_name;
}
