// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/eol_notice_quick_settings_view.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/update/quick_settings_notice_view.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

EolNoticeQuickSettingsView::EolNoticeQuickSettingsView()
    : QuickSettingsNoticeView(
          VIEW_ID_QS_EOL_NOTICE_BUTTON,
          QsButtonCatalogName::kEolNoticeButton,
          IDS_ASH_QUICK_SETTINGS_BUBBLE_EOL_NOTICE,
          kUpgradeIcon,
          base::BindRepeating([](const ui::Event& event) {
            Shell::Get()->system_tray_model()->client()->ShowEolInfoPage();
          })) {
  Shell::Get()->system_tray_model()->client()->RecordEolNoticeShown();
}

EolNoticeQuickSettingsView::~EolNoticeQuickSettingsView() = default;

int EolNoticeQuickSettingsView::GetShortTextId() const {
  return IDS_ASH_QUICK_SETTINGS_BUBBLE_EOL_NOTICE_SHORT;
}

BEGIN_METADATA(EolNoticeQuickSettingsView)
END_METADATA

}  // namespace ash
