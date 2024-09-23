// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/extended_updates_notice_quick_settings_view.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/extended_updates/extended_updates_metrics.h"
#include "ash/system/model/system_tray_model.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

ExtendedUpdatesNoticeQuickSettingsView::ExtendedUpdatesNoticeQuickSettingsView()
    : QuickSettingsNoticeView(
          VIEW_ID_QS_EXTENDED_UPDATES_NOTICE_BUTTON,
          QsButtonCatalogName::kExtendedUpdatesNoticeButton,
          IDS_ASH_QUICK_SETTINGS_BUBBLE_EXTENDED_UPDATES_NOTICE,
          kUnifiedMenuInfoIcon,
          base::BindRepeating([](const ui::Event& event) {
            RecordExtendedUpdatesEntryPointEvent(
                ExtendedUpdatesEntryPointEvent::kQuickSettingsBannerClicked);
            Shell::Get()->system_tray_model()->client()->ShowAboutChromeOS();
          })) {
  RecordExtendedUpdatesEntryPointEvent(
      ExtendedUpdatesEntryPointEvent::kQuickSettingsBannerShown);
}

ExtendedUpdatesNoticeQuickSettingsView::
    ~ExtendedUpdatesNoticeQuickSettingsView() = default;

BEGIN_METADATA(ExtendedUpdatesNoticeQuickSettingsView)
END_METADATA

}  // namespace ash
