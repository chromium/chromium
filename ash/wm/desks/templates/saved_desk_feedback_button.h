// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_FEEDBACK_BUTTON_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_FEEDBACK_BUTTON_H_

#include "ash/style/pill_button.h"
#include "ash/wm/overview/overview_highlightable_view.h"

namespace ash {

class ASH_EXPORT FeedbackButton : public PillButton,
                                  public OverviewHighlightableView {
 public:
  FeedbackButton(base::RepeatingClosure callback,
                 const std::u16string& text,
                 Type type,
                 const gfx::VectorIcon* icon);
  FeedbackButton(const FeedbackButton&) = delete;
  FeedbackButton& operator=(const FeedbackButton&) = delete;
  ~FeedbackButton() override;

  // OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView(bool primary_action) override {}
  void MaybeSwapHighlightedView(bool right) override {}
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

 private:
  base::RepeatingClosure callback_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_FEEDBACK_BUTTON_H_
