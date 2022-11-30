// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_BUTTON_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

// A button used in the WindowCycleTabSlider to choose between
// all desks and current desks mode.
class ASH_EXPORT WindowCycleTabSliderButton : public views::LabelButton {
 public:
  METADATA_HEADER(WindowCycleTabSliderButton);

  WindowCycleTabSliderButton(views::Button::PressedCallback callback,
                             const std::u16string& label);
  WindowCycleTabSliderButton(const WindowCycleTabSliderButton&) = delete;
  WindowCycleTabSliderButton& operator=(const WindowCycleTabSliderButton&) =
      delete;
  ~WindowCycleTabSliderButton() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  void SetToggled(bool is_toggled);

 private:
  bool toggled_ = false;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_BUTTON_H_
