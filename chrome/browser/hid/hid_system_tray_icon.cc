// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_system_tray_icon.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/device_notifications/device_system_tray_icon_renderer.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

// static
const gfx::VectorIcon& HidSystemTrayIcon::GetIcon() {
  return vector_icons::kVideogameAssetIcon;
}

// static
std::u16string HidSystemTrayIcon::GetTitleLabel(size_t num_origins,
                                                size_t num_connections) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return l10n_util::GetPluralStringFUTF16(IDS_WEBHID_SYSTEM_TRAY_ICON_TITLE,
                                          static_cast<int>(num_connections));
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

// static
std::u16string HidSystemTrayIcon::GetContentSettingsLabel() {
  return l10n_util::GetStringUTF16(IDS_WEBHID_SYSTEM_TRAY_ICON_HID_SETTINGS);
}

HidSystemTrayIcon::HidSystemTrayIcon(
    std::unique_ptr<DeviceSystemTrayIconRenderer> icon_renderer)
    : DeviceSystemTrayIcon(std::move(icon_renderer)) {}

HidSystemTrayIcon::~HidSystemTrayIcon() = default;

DeviceConnectionTracker* HidSystemTrayIcon::GetConnectionTracker(
    base::WeakPtr<Profile> profile) {
  if (!profile) {
    return nullptr;
  }
  return HidConnectionTrackerFactory::GetForProfile(profile.get(),
                                                    /*create=*/false);
}
