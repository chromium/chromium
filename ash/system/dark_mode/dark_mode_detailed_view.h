// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DARK_MODE_DARK_MODE_DETAILED_VIEW_H_
#define ASH_SYSTEM_DARK_MODE_DARK_MODE_DETAILED_VIEW_H_

#include "ash/system/tray/tray_detailed_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
class Label;
class RadioButton;
class ToggleButton;
}  // namespace views

namespace ash {

// This view displays options to switch between themed and neutral
// color mode for the system. Accessed by clicking on the dark mode
// feature pod label button.
class DarkModeDetailedView : public TrayDetailedView {
 public:
  METADATA_HEADER(DarkModeDetailedView);

  explicit DarkModeDetailedView(DetailedViewDelegate* delegate);
  DarkModeDetailedView(const DarkModeDetailedView& other) = delete;
  DarkModeDetailedView& operator=(const DarkModeDetailedView& other) = delete;
  ~DarkModeDetailedView() override;

  void OnThemeChanged() override;

  // Updates the status of |toggle_| on |dark_mode_enabled|.
  void UpdateToggleButton(bool dark_mode_enabled);

  // Updates the currently checked radio button.
  void UpdateCheckedButton(bool is_themed);

 private:
  void CreateItems();

  views::ToggleButton* toggle_ = nullptr;
  views::RadioButton* themed_mode_button_ = nullptr;
  views::RadioButton* neutral_mode_button_ = nullptr;
  views::Label* themed_label_ = nullptr;
  views::Label* neutral_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_DARK_MODE_DARK_MODE_DETAILED_VIEW_H_
