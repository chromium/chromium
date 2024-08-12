// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_SCROLL_ARROW_BUTTON_H_
#define ASH_WM_DESKS_SCROLL_ARROW_BUTTON_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class DeskBarViewBase;

// The scroll button used by scrollable desks bar in Bento. An arrow icon will
// be added to the button. But Button used here instead of ImageButton since we
// want to paint the button on arrow type and RTL layout differently, also
// customize the icon's layout.
class ASH_EXPORT ScrollArrowButton : public views::Button {
  METADATA_HEADER(ScrollArrowButton, views::Button)

 public:
  ScrollArrowButton(base::RepeatingClosure on_scroll,
                    bool is_left_arrow,
                    DeskBarViewBase* bar_view);
  ScrollArrowButton(const ScrollArrowButton&) = delete;
  ScrollArrowButton& operator=(const ScrollArrowButton&) = delete;
  ~ScrollArrowButton() override;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // Called when a desk is being dragged and hovering on the button.
  void OnDeskHoverStart();
  // Called when the desk is dropped or leaves the button.
  void OnDeskHoverEnd();

 private:
  friend class DesksTestApi;

  void OnStateChanged();

  static base::AutoReset<base::TimeDelta> SetScrollTimeIntervalForTest(
      base::TimeDelta interval);

  // The callback of bar scroll method.
  base::RepeatingClosure on_scroll_;
  // The subscription of button state change callback.
  base::CallbackListSubscription state_change_subscription_;
  const bool is_left_arrow_;
  const raw_ptr<DeskBarViewBase> bar_view_;  // Not owned.
  // If the button is kept pressed, trigger scroll every one second.
  base::RepeatingTimer timer_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_SCROLL_ARROW_BUTTON_H_
