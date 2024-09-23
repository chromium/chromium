// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UPDATE_EXTENDED_UPDATES_NOTICE_QUICK_SETTINGS_VIEW_H_
#define ASH_SYSTEM_UPDATE_EXTENDED_UPDATES_NOTICE_QUICK_SETTINGS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/update/quick_settings_notice_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Label button shown in the quick settings header when the device is eligible
// to receive extended updates support.
// Clicking on the button will bring the user to the About ChromeOS
// settings page to set up extended updates support.
class ASH_EXPORT ExtendedUpdatesNoticeQuickSettingsView
    : public QuickSettingsNoticeView {
  METADATA_HEADER(ExtendedUpdatesNoticeQuickSettingsView,
                  QuickSettingsNoticeView)

 public:
  ExtendedUpdatesNoticeQuickSettingsView();
  ExtendedUpdatesNoticeQuickSettingsView(
      const ExtendedUpdatesNoticeQuickSettingsView&) = delete;
  ExtendedUpdatesNoticeQuickSettingsView& operator=(
      const ExtendedUpdatesNoticeQuickSettingsView&) = delete;
  ~ExtendedUpdatesNoticeQuickSettingsView() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UPDATE_EXTENDED_UPDATES_NOTICE_QUICK_SETTINGS_VIEW_H_
