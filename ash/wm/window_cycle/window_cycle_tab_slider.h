// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_cycle/window_cycle_tab_slider_button.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace ash {

// A WindowCycleTabSlider containing two buttons to switch between
// all desks and current desks mode.
class ASH_EXPORT WindowCycleTabSlider : public views::View {
 public:
  METADATA_HEADER(WindowCycleTabSlider);

  WindowCycleTabSlider();
  WindowCycleTabSlider(const WindowCycleTabSlider&) = delete;
  WindowCycleTabSlider& operator=(const WindowCycleTabSlider&) = delete;
  ~WindowCycleTabSlider() override = default;

  void OnModeChanged(bool per_desk);

  // TODO(crbug.com/1157087): Add tab slider animation.

 private:
  bool per_desk_mode_ = false;

  WindowCycleTabSliderButton* all_desks_tab_slider_button_;
  WindowCycleTabSliderButton* current_desk_tab_slider_button_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_
