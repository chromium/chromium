// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BUTTON_BASE_H_
#define ASH_WM_DESKS_DESK_BUTTON_BASE_H_

#include "ash/wm/overview/overview_focusable_view.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

class DeskBarViewBase;

// The base class of buttons (default desk button, new desk button and library
// button) on desks bar. It's guaranteed this button always lives under a desk
// bar view.
class DeskButtonBase : public views::LabelButton, public OverviewFocusableView {
 public:
  METADATA_HEADER(DeskButtonBase);

  explicit DeskButtonBase(const std::u16string& text,
                          bool set_text,
                          DeskBarViewBase* bar_view,
                          base::RepeatingClosure pressed_callback);
  DeskButtonBase(const DeskButtonBase&) = delete;
  DeskButtonBase& operator=(const DeskButtonBase&) = delete;
  ~DeskButtonBase() override;

  // views::LabelButton:
  void OnFocus() override;
  void OnBlur() override;

  // OverviewFocusableView:
  views::View* GetView() override;
  void MaybeActivateFocusedView() override;
  void MaybeCloseFocusedView(bool primary_action) override;
  void MaybeSwapFocusedView(bool right) override;
  void OnFocusableViewFocused() override;
  void OnFocusableViewBlurred() override;

 protected:
  virtual void UpdateFocusState();

  // Owned by the views hierarchy.
  const raw_ptr<DeskBarViewBase> bar_view_;

 private:
  base::RepeatingClosure pressed_callback_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BUTTON_BASE_H_
