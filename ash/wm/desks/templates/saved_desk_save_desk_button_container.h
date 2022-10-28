// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_SAVE_DESK_BUTTON_CONTAINER_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_SAVE_DESK_BUTTON_CONTAINER_H_

#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"

#include "base/functional/callback.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class ASH_EXPORT SavedDeskSaveDeskButtonContainer
    : public views::BoxLayoutView {
 public:
  SavedDeskSaveDeskButtonContainer(
      base::RepeatingClosure save_as_template_callback,
      base::RepeatingClosure save_for_later_callback);
  SavedDeskSaveDeskButtonContainer(const SavedDeskSaveDeskButtonContainer&) =
      delete;
  SavedDeskSaveDeskButtonContainer& operator=(
      const SavedDeskSaveDeskButtonContainer&) = delete;

  ~SavedDeskSaveDeskButtonContainer() override;

  SavedDeskSaveDeskButton* save_desk_as_template_button() {
    return save_desk_as_template_button_;
  }

  SavedDeskSaveDeskButton* save_desk_for_later_button() {
    return save_desk_for_later_button_;
  }

  void UpdateButtonEnableStateAndTooltip(
      SavedDeskSaveDeskButton::Type button_type,
      int current_entry_count,
      int max_entry_count,
      int incognito_window_count,
      int unsupported_window_count,
      int window_count);

  void UpdateButtonContainerForAccessibilityState();

 private:
  class SaveDeskButtonContainerAccessibilityObserver;

  SavedDeskSaveDeskButton* GetButtonFromType(
      SavedDeskSaveDeskButton::Type type);

  SavedDeskSaveDeskButton* save_desk_as_template_button_ = nullptr;
  SavedDeskSaveDeskButton* save_desk_for_later_button_ = nullptr;

  // Object responsible for observing accessibility setting changes.
  std::unique_ptr<SaveDeskButtonContainerAccessibilityObserver>
      accessibility_observer_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_SAVE_DESK_BUTTON_CONTAINER_H_
