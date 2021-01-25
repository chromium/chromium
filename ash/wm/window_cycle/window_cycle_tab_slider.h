// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_cycle/window_cycle_tab_slider_button.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace gfx {
class Size;
}

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

  // Updates user prefs when users switch the button.
  void OnModeChanged(bool per_desk);

  // Updates UI when user prefs change.
  void OnModePrefsChanged();

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  const views::View::Views& GetTabSliderButtonsForTesting() const;

 private:
  // The view that acts as the background for the currently active mode's
  // button. It is animated during mode change.
  views::View* active_button_background_;

  // The view that contains the tab slider buttons.
  views::View* buttons_container_;

  WindowCycleTabSliderButton* all_desks_tab_slider_button_;
  WindowCycleTabSliderButton* current_desk_tab_slider_button_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_
