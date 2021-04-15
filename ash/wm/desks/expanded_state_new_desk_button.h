// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_EXPANDED_STATE_NEW_DESK_BUTTON_H_
#define ASH_WM_DESKS_EXPANDED_STATE_NEW_DESK_BUTTON_H_

#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class DeskButtonBase;
class DesksBarView;

// The new desk button view in the expanded desks bar. It includes the
// InnerNewDeskButton and a name label below, which has the same style as a
// DeskMiniVIew. But the name label is not changeable and not focusable.
class ExpandedStateNewDeskButton : public views::View {
 public:
  ExpandedStateNewDeskButton(DesksBarView* bar_view);
  ExpandedStateNewDeskButton(const ExpandedStateNewDeskButton&) = delete;
  ExpandedStateNewDeskButton& operator=(const ExpandedStateNewDeskButton&) =
      delete;
  ~ExpandedStateNewDeskButton() override = default;

  // views::View:
  void Layout() override;

  // Updates |new_desk_button_|'s state on current desks state.
  void UpdateButtonState();

  // Updates the |label_|'s color on DesksController::CanCreateDesks.
  void UpdateLabelColor();

  DeskButtonBase* new_desk_button() { return new_desk_button_; }

 private:
  DesksBarView* bar_view_;  // Not owned.
  DeskButtonBase* new_desk_button_;
  views::Label* label_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_EXPANDED_STATE_NEW_DESK_BUTTON_H_
