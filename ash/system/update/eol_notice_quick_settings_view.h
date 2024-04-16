// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UPDATE_EOL_NOTICE_QUICK_SETTINGS_VIEW_H_
#define ASH_SYSTEM_UPDATE_EOL_NOTICE_QUICK_SETTINGS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/update/quick_settings_notice_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Label button shown in the quick settings when the device has reached end of
// life. Clicking the label will request a page with more info about device end
// of life to be opened.
class ASH_EXPORT EolNoticeQuickSettingsView : public QuickSettingsNoticeView {
  METADATA_HEADER(EolNoticeQuickSettingsView, QuickSettingsNoticeView)

 public:
  EolNoticeQuickSettingsView();
  ~EolNoticeQuickSettingsView() override;

  EolNoticeQuickSettingsView(const EolNoticeQuickSettingsView&) = delete;
  EolNoticeQuickSettingsView& operator=(const EolNoticeQuickSettingsView&) =
      delete;

 protected:
  // QuickSettingsNoticeView:
  int GetShortTextId() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UPDATE_EOL_NOTICE_QUICK_SETTINGS_VIEW_H_
