// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_CROS_NEXT_DESK_BUTTON_BASE_H_
#define ASH_WM_DESKS_CROS_NEXT_DESK_BUTTON_BASE_H_

#include "ash/wm/overview/overview_focusable_view.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

class DeskBarViewBase;

// The base class of buttons (default desk button, new desk button and library
// button) on desks bar. It's guaranteed this button always lives under a desk
// bar view.
// TODO(https://b/291622042): Remove `DeskButtonBase`, replace it with this
// class and rename this class by removing the prefix CrOSNext.
class CrOSNextDeskButtonBase : public views::LabelButton,
                               public OverviewFocusableView {
 public:
  METADATA_HEADER(CrOSNextDeskButtonBase);

  explicit CrOSNextDeskButtonBase(const std::u16string& text,
                                  bool set_text,
                                  DeskBarViewBase* bar_view,
                                  base::RepeatingClosure pressed_callback);
  CrOSNextDeskButtonBase(const CrOSNextDeskButtonBase&) = delete;
  CrOSNextDeskButtonBase& operator=(const CrOSNextDeskButtonBase&) = delete;
  ~CrOSNextDeskButtonBase() override;

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
  const raw_ptr<DeskBarViewBase, ExperimentalAsh> bar_view_;

 private:
  base::RepeatingClosure pressed_callback_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_CROS_NEXT_DESK_BUTTON_BASE_H_
