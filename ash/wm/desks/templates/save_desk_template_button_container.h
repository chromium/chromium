// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVE_DESK_TEMPLATE_BUTTON_CONTAINER_H_
#define ASH_WM_DESKS_TEMPLATES_SAVE_DESK_TEMPLATE_BUTTON_CONTAINER_H_

#include "ash/wm/desks/templates/save_desk_template_button.h"
#include "base/callback.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class ASH_EXPORT SaveDeskTemplateButtonContainer : public views::BoxLayoutView {
 public:
  SaveDeskTemplateButtonContainer(
      base::RepeatingClosure save_as_template_callback,
      base::RepeatingClosure save_for_later_callback);
  SaveDeskTemplateButtonContainer(const SaveDeskTemplateButtonContainer&) =
      delete;
  SaveDeskTemplateButtonContainer& operator=(
      const SaveDeskTemplateButtonContainer&) = delete;

  SaveDeskTemplateButton* save_desk_as_template_button() {
    return save_desk_as_template_button_;
  }

  SaveDeskTemplateButton* save_desk_for_later_button() {
    return save_desk_for_later_button_;
  }

  void UpdateButtonEnableStateAndTooltip(
      SaveDeskTemplateButton::Type button_type,
      int current_entry_count,
      int max_entry_count,
      int incognito_window_count,
      int unsupported_window_count,
      int window_count);

 private:
  SaveDeskTemplateButton* GetButtonFromType(SaveDeskTemplateButton::Type type);

  SaveDeskTemplateButton* save_desk_as_template_button_ = nullptr;
  SaveDeskTemplateButton* save_desk_for_later_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVE_DESK_TEMPLATE_BUTTON_CONTAINER_H_
