// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider_handler_view.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/shell.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ash {

class SplitViewDividerHandlerView::SelectionAnimation
    : public gfx::SlideAnimation,
      public gfx::AnimationDelegate {
 public:
  SelectionAnimation(SplitViewDividerHandlerView* white_handler_view)
      : gfx::SlideAnimation(this), white_handler_view_(white_handler_view) {
    SetSlideDuration(kSplitviewDividerSelectionStatusChangeDuration);
    SetTweenType(gfx::Tween::EASE_IN);
  }

  ~SelectionAnimation() override = default;

  void UpdateWhiteHandlerBounds() {
    white_handler_view_->SetBounds(
        CurrentValueBetween(kSplitviewWhiteBarShortSideLength,
                            kSplitviewWhiteBarRadius * 2),
        CurrentValueBetween(kSplitviewWhiteBarLongSideLength,
                            kSplitviewWhiteBarRadius * 2),
        /*signed_offset=*/0);
  }

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    UpdateWhiteHandlerBounds();
    white_handler_view_->SetCornerRadius(CurrentValueBetween(
        kSplitviewWhiteBarCornerRadius, kSplitviewWhiteBarRadius));
  }

  SplitViewDividerHandlerView* white_handler_view_;

  DISALLOW_COPY_AND_ASSIGN(SelectionAnimation);
};

class SplitViewDividerHandlerView::SpawningAnimation
    : public gfx::SlideAnimation,
      public gfx::AnimationDelegate {
 public:
  SpawningAnimation(SplitViewDividerHandlerView* white_handler_view,
                    int divider_signed_offset)
      : gfx::SlideAnimation(this),
        white_handler_view_(white_handler_view),
        spawn_signed_offset_(divider_signed_offset +
                             (divider_signed_offset > 0
                                  ? kSplitviewWhiteBarSpawnUnsignedOffset
                                  : -kSplitviewWhiteBarSpawnUnsignedOffset)) {
    SetSlideDuration(kSplitviewDividerSpawnDuration);
    SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
  }

  ~SpawningAnimation() override = default;

  void Activate() {
    white_handler_view_->SetVisible(false);
    delay_timer_.Start(FROM_HERE, kSplitviewDividerSpawnDelay, this,
                       &SpawningAnimation::StartAnimation);
  }

  bool IsActive() const { return delay_timer_.IsRunning() || is_animating(); }

  void UpdateWhiteHandlerBounds() {
    DCHECK(IsActive());
    white_handler_view_->SetBounds(
        kSplitviewWhiteBarShortSideLength,
        CurrentValueBetween(kSplitviewWhiteBarSpawnLongSideLength,
                            kSplitviewWhiteBarLongSideLength),
        CurrentValueBetween(spawn_signed_offset_, 0));
  }

 private:
  void StartAnimation() {
    DCHECK(!white_handler_view_->GetVisible());
    white_handler_view_->SetVisible(true);
    DCHECK(!is_animating());
    Show();
    DCHECK_EQ(0.0, GetCurrentValue());
    UpdateWhiteHandlerBounds();
  }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    UpdateWhiteHandlerBounds();
  }

  SplitViewDividerHandlerView* white_handler_view_;
  int spawn_signed_offset_;
  base::OneShotTimer delay_timer_;

  DISALLOW_COPY_AND_ASSIGN(SpawningAnimation);
};

SplitViewDividerHandlerView::SplitViewDividerHandlerView()
    : RoundedRectView(kSplitviewWhiteBarCornerRadius, kSplitviewWhiteBarColor),
      selection_animation_(std::make_unique<SelectionAnimation>(this)) {
  SetPaintToLayer();
}

SplitViewDividerHandlerView::~SplitViewDividerHandlerView() = default;

void SplitViewDividerHandlerView::DoSpawningAnimation(
    int divider_signed_offset) {
  spawning_animation_ =
      std::make_unique<SpawningAnimation>(this, divider_signed_offset);
  spawning_animation_->Activate();
}

void SplitViewDividerHandlerView::Refresh(bool is_resizing) {
  spawning_animation_.reset();
  SetVisible(true);
  selection_animation_->UpdateWhiteHandlerBounds();
  if (is_resizing)
    selection_animation_->Show();
  else
    selection_animation_->Hide();
}

void SplitViewDividerHandlerView::SetBounds(int short_length,
                                            int long_length,
                                            int signed_offset) {
  const bool landscape = IsCurrentScreenOrientationLandscape();
  gfx::Rect bounds = landscape ? gfx::Rect(short_length, long_length)
                               : gfx::Rect(long_length, short_length);
  bounds.Offset(parent()->GetLocalBounds().CenterPoint() -
                bounds.CenterPoint() +
                (landscape ? gfx::Vector2d(signed_offset, 0)
                           : gfx::Vector2d(0, signed_offset)));
  SetBoundsRect(bounds);
}

void SplitViewDividerHandlerView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  // It's needed to avoid artifacts when tapping on the divider quickly.
  canvas->DrawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);
  RoundedRectView::OnPaint(canvas);
}

}  // namespace ash
