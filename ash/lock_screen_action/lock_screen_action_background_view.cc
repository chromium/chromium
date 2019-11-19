// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_action_background_view.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

class LockScreenActionBackgroundView::NoteBackground
    : public views::InkDropHostView {
 public:
  explicit NoteBackground(views::InkDropObserver* observer)
      : observer_(observer) {
    DCHECK(observer);
    SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  }

  ~NoteBackground() override = default;

  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop =
        CreateDefaultFloodFillInkDropImpl();
    ink_drop->SetShowHighlightOnHover(false);
    ink_drop->SetShowHighlightOnFocus(false);
    ink_drop->SetAutoHighlightMode(views::InkDropImpl::AutoHighlightMode::NONE);
    ink_drop->AddObserver(observer_);
    return std::move(ink_drop);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    gfx::Point center = base::i18n::IsRTL() ? GetLocalBounds().origin()
                                            : GetLocalBounds().top_right();
    auto ink_drop_ripple = std::make_unique<views::FloodFillInkDropRipple>(
        size(), gfx::Insets(), center, GetInkDropBaseColor(), 1);
    ink_drop_ripple->set_use_hide_transform_duration_for_hide_fade_out(true);
    ink_drop_ripple->set_duration_factor(1.5);
    return ink_drop_ripple;
  }

  SkColor GetInkDropBaseColor() const override { return SK_ColorBLACK; }

 private:
  views::InkDropObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(NoteBackground);
};

LockScreenActionBackgroundView::LockScreenActionBackgroundView() {
  auto layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  auto* layout_ptr = SetLayoutManager(std::move(layout_manager));

  background_ = new NoteBackground(this);
  AddChildView(background_);
  // Make background view flexible - the constant does not really matter given
  // that |background_| is the only child, as long as it's greater than 0.
  layout_ptr->SetFlexForView(background_, 1 /*flex_weight*/);
}

LockScreenActionBackgroundView::~LockScreenActionBackgroundView() = default;

void LockScreenActionBackgroundView::AnimateShow(base::OnceClosure done) {
  animation_end_callback_ = std::move(done);
  animating_to_state_ = views::InkDropState::ACTIVATED;

  background_->AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
}

void LockScreenActionBackgroundView::AnimateHide(base::OnceClosure done) {
  animation_end_callback_ = std::move(done);
  animating_to_state_ = views::InkDropState::HIDDEN;

  background_->AnimateInkDrop(views::InkDropState::HIDDEN, nullptr);
}

void LockScreenActionBackgroundView::InkDropAnimationStarted() {}

void LockScreenActionBackgroundView::InkDropRippleAnimationEnded(
    views::InkDropState state) {
  // In case |AnimateShow| or |AnimateHide| is called before previous state
  // animation ends, this might get called with the previous target state
  // as the animation is aborted - ignore the event if the |state| does not
  // match the current target state.
  if (animation_end_callback_.is_null() || state != animating_to_state_)
    return;

  std::move(animation_end_callback_).Run();
}

bool LockScreenActionBackgroundView::CanMaximize() const {
  return true;
}

bool LockScreenActionBackgroundView::CanActivate() const {
  return false;
}

views::View* LockScreenActionBackgroundView::GetBackgroundView() {
  return background_;
}

}  // namespace ash
