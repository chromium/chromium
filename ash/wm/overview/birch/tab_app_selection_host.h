// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_HOST_H_
#define ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_HOST_H_

#include "ui/views/widget/widget.h"

namespace ash {

class CoralChipButton;
class ScopedA11yOverrideWindowSetter;

// The host which contains the TabAppSelectionView. This widget slides in when
// shown.
class TabAppSelectionHost : public views::Widget {
 public:
  explicit TabAppSelectionHost(CoralChipButton* coral_chip);
  TabAppSelectionHost(const TabAppSelectionHost&) = delete;
  TabAppSelectionHost& operator=(const TabAppSelectionHost&) = delete;
  ~TabAppSelectionHost() override;

  void ProcessKeyEvent(ui::KeyEvent* event);

  // Called when an item is removed from the selection view. Reloads the chip
  // button image.
  void OnItemRemoved();

  // Slides the widget under `owner_` before hiding it.
  void SlideOut();

  // Removes an item associated with given `identifier` from selection view.
  void RemoveItem(std::string_view identifier);

  // views::Widget:
  void OnNativeWidgetVisibilityChanged(bool visible) override;

  const CoralChipButton* owner_for_testing() const { return owner_; }

 private:
  class SelectionHostHider;

  gfx::Rect GetDesiredBoundsInScreen();

  // Handles the focus moving by Tab key. Tab app selector has different
  // navigation behaviors for the Tab key and arrow key. The arrow key
  // navigation will be handled by
  // `TabAppSelectionView::AdvanceFocusForArrowKey`.
  void AdvanceFocusForTabKey(bool reverse);

  std::unique_ptr<SelectionHostHider> hider_;
  const raw_ptr<CoralChipButton> owner_;

  // Used for metrics.
  int number_of_removed_items_ = 0;

  // This widget isn't activatable so this is a way to force accessibility
  // features to focus on the underlying window.
  std::unique_ptr<ScopedA11yOverrideWindowSetter> scoped_a11y_overrider_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_HOST_H_
