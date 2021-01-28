// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_cycle/window_cycle_tab_slider_button.h"
#include "ash/wm/wm_highlight_item_border.h"
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

  enum ModeSwitchSource { BUTTON, KEYBOARD, USER_PREFS };

  WindowCycleTabSlider();
  WindowCycleTabSlider(const WindowCycleTabSlider&) = delete;
  WindowCycleTabSlider& operator=(const WindowCycleTabSlider&) = delete;
  ~WindowCycleTabSlider() override = default;

  // Updates user prefs when users switch the button.
  void OnModeChanged(bool per_desk,
                     WindowCycleTabSlider::ModeSwitchSource source);

  // Displays or hides the highlight on the active button selector during
  // keyboard navigation.
  void SetHighlightVisibility(bool focus);

  // views::View:
  // void OnPaintBackground(gfx::Canvas* canvas) override;

  // Updates UI when user prefs change.
  void OnModePrefsChanged();

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  const views::View::Views& GetTabSliderButtonsForTesting() const;

 private:
  // Updates the active button selector with moving animation from the
  // currently selected button to the target button representing |per_desk|
  // mode.
  void UpdateActiveButtonSelector(bool per_desk);

  // Returns an equalized button size calculated from maximum width and height
  // of the prefer size of all buttons.
  gfx::Size GetPreferredSizeForButtons();

  // The view that acts as an active button selector to show the active button
  // background and the highlight border if applicable. It is animated during
  // mode change.
  views::View* active_button_selector_;

  // The highlight border, the focus ring, of the active button selector.
  WmHighlightItemBorder* highlight_border_;

  // The view that contains the tab slider buttons.
  views::View* buttons_container_;

  WindowCycleTabSliderButton* all_desks_tab_slider_button_;
  WindowCycleTabSliderButton* current_desk_tab_slider_button_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_TAB_SLIDER_H_
