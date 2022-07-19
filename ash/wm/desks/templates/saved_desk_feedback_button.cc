// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_feedback_button.h"

#include "ash/style/style_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "base/check.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

FeedbackButton::FeedbackButton(base::RepeatingClosure callback,
                               const std::u16string& text,
                               Type type,
                               const gfx::VectorIcon* icon)
    : PillButton(callback, text, type, icon), callback_(callback) {
  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kFocusRingHaloInset);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return static_cast<FeedbackButton*>(view)->IsViewHighlighted();
  });
  focus_ring->SetColorId(ui::kColorAshFocusRing);
}

FeedbackButton::~FeedbackButton() = default;

views::View* FeedbackButton::GetView() {
  return this;
}

void FeedbackButton::MaybeActivateHighlightedView() {
  DCHECK(callback_);
  callback_.Run();
}

void FeedbackButton::OnViewHighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();

  ScrollViewToVisible();
}

void FeedbackButton::OnViewUnhighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

}  // namespace ash
