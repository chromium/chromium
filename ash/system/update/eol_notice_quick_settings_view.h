// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UPDATE_EOL_NOTICE_QUICK_SETTINGS_VIEW_H_
#define ASH_SYSTEM_UPDATE_EOL_NOTICE_QUICK_SETTINGS_VIEW_H_

#include "ui/views/controls/button/label_button.h"

namespace ash {

// Label button shown in the quick settings when the device has reached end of
// life. Clicking the label will request a page with more info about device end
// of life to be opened.
class EolNoticeQuickSettingsView : public views::LabelButton {
 public:
  EolNoticeQuickSettingsView();
  ~EolNoticeQuickSettingsView() override;

  EolNoticeQuickSettingsView(const EolNoticeQuickSettingsView&) = delete;
  EolNoticeQuickSettingsView& operator=(const EolNoticeQuickSettingsView&) =
      delete;

  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UPDATE_EOL_NOTICE_QUICK_SETTINGS_VIEW_H_
