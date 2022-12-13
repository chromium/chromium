// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_system_tray_icon.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

// static
gfx::ImageSkia HidSystemTrayIcon::GetStatusTrayIcon() {
  return gfx::CreateVectorIcon(vector_icons::kVideogameAssetIcon,
                               gfx::kGoogleGrey300);
}

// static
std::u16string HidSystemTrayIcon::GetManageHidDeviceButtonLabel(
    Profile* profile) {
  std::u16string profile_name =
      base::UTF8ToUTF16(profile->GetProfileUserName());
  if (profile_name.empty()) {
    return l10n_util::GetStringUTF16(
        IDS_WEBHID_SYSTEM_TRAY_ICON_BUTTON_FOR_MANAGE_HID_DEVICE);
  }
  return l10n_util::GetStringFUTF16(
      IDS_WEBHID_SYSTEM_TRAY_ICON_BUTTON_FOR_MANAGE_HID_DEVICE_WITH_PROFILE_NAME,
      profile_name);
}

// static
std::u16string HidSystemTrayIcon::GetTooltipLabel(size_t num_devices) {
  return l10n_util::GetPluralStringFUTF16(IDS_WEBHID_SYSTEM_TRAY_ICON_TOOLTIP,
                                          static_cast<int>(num_devices));
}
