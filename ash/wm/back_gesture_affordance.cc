// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/back_gesture_affordance.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Distance from the arrow to the drag touch point. The arrow is usually
// above the touch point but will be put below the touch point if the affordance
// is outside of the display.
constexpr int kDistanceFromArrowToTouchPoint = 64;

// Parameters defining the arrow of the affordance.
constexpr int kArrowSize = 20;
constexpr SkColor kArrowColorBeforeActivated = gfx::kGoogleBlue600;
constexpr SkColor kArrowColorAfterActivated = gfx::kGoogleGrey100;

// Parameters defining the background circle of the affordance.
constexpr int kBackgroundRadius = 20;
const SkColor kBackgroundColorBeforeActivated = SK_ColorWHITE;
const SkColor kBackgroundColorAfterActivated = gfx::kGoogleBlue600;
// Y offset of the background shadow.
const int kBgShadowOffsetY = 2;
const int kBgShadowBlurRadius = 8;
const SkColor kBgShadowColor = SkColorSetA(SK_ColorBLACK, 0x4D);

// Radius of the ripple while x-axis movement of the affordance achieves
// |kDistanceForFullRadius|.
constexpr int kFullRippleRadius = 32;

// Radius of the ripple while x-axis movement of the affordance achieves
// |kDistanceForMaxRadius|.
constexpr int kMaxRippleRadius = 40;

// Maximium burst ripple radius while release the drag with COMPLETING state.
constexpr int kMaxBurstRippleRadius = 48;

// X-axis movement of the affordance to achieve full ripple radius. Note, the
// movement equals to drag distance while in this range.
constexpr float kDistanceForFullRadius = 100.f;

// |kDistanceForFullRadius| plus the x-axis movement of the affordance for
// ripple to increase from |kFullRippleRadius| to |kMaxRippleRadius|.
constexpr float kDistanceForMaxRadius = 116.f;

// Drag distance of the gesture events for the ripple radius to achieve
// |kMaxRippleRadius|.
constexpr float kDragDistanceForMaxRadius = 260.f;

constexpr base::TimeDelta kAbortAnimationTimeout =
    base::TimeDelta::FromMilliseconds(300);
constexpr base::TimeDelta kCompleteAnimationTimeout =
    base::TimeDelta::FromMilliseconds(200);

constexpr SkColor kRippleColor = SkColorSetA(gfx::kGoogleBlue600, 0x4C);  // 30%

// Y-axis drag distance to achieve full y drag progress.
constexpr float kDistanceForFullYProgress = 80.f;

// Maximium y-axis movement of the affordance. Note, the affordance can move
// both up and down.
constexpr float kMaxYMovement = 8.f;

class AffordanceView : public views::View {
 public:
  AffordanceView() {
    SetPaintToLayer(ui::LAYER_TEXTURED);
    layer()->SetFillsBoundsOpaquely(false);
  }

  ~AffordanceView() override = default;

  // Schedule painting on given |affordance_progress|, |complete_progress| and
  // |state|.
  void Paint(float complete_progress,
             BackGestureAffordance::State state,
             float x_offset) {
    complete_progress_ = complete_progress;
    state_ = state;
    x_offset_ = x_offset;
    SchedulePaint();
  }

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    const gfx::PointF center_point(kMaxBurstRippleRadius,
                                   kMaxBurstRippleRadius);

    // Draw the ripple.
    cc::PaintFlags ripple_flags;
    ripple_flags.setAntiAlias(true);
    ripple_flags.setStyle(cc::PaintFlags::kFill_Style);
    ripple_flags.setColor(kRippleColor);

    const bool is_activated = x_offset_ >= kDistanceForFullRadius;
    float ripple_radius = 0.f;
    if (state_ == BackGestureAffordance::State::COMPLETING) {
      const float burst_progress = gfx::Tween::CalculateValue(
          gfx::Tween::FAST_OUT_SLOW_IN, complete_progress_);
      ripple_radius =
          kMaxRippleRadius +
          burst_progress * (kMaxBurstRippleRadius - kMaxRippleRadius);
    } else if (is_activated) {
      const float factor = (kMaxRippleRadius - kFullRippleRadius) /
                           (kDistanceForMaxRadius - kDistanceForFullRadius);
      ripple_radius = (kFullRippleRadius - factor * kDistanceForFullRadius) +
                      factor * x_offset_;
    } else {
      const float factor =
          (kFullRippleRadius - kBackgroundRadius) / kDistanceForFullRadius;
      ripple_radius = kBackgroundRadius + factor * x_offset_;
    }
    canvas->DrawCircle(center_point, ripple_radius, ripple_flags);

    // Draw the arrow background circle.
    cc::PaintFlags bg_flags;
    bg_flags.setAntiAlias(true);
    bg_flags.setStyle(cc::PaintFlags::kFill_Style);
    gfx::ShadowValues shadow;
    shadow.emplace_back(gfx::Vector2d(0, kBgShadowOffsetY), kBgShadowBlurRadius,
                        kBgShadowColor);
    bg_flags.setLooper(gfx::CreateShadowDrawLooper(shadow));
    bg_flags.setColor(is_activated ? kBackgroundColorAfterActivated
                                   : kBackgroundColorBeforeActivated);
    canvas->DrawCircle(center_point, kBackgroundRadius, bg_flags);

    // Draw the arrow.
    const float arrow_x = center_point.x() - kArrowSize / 2.f;
    const float arrow_y = center_point.y() - kArrowSize / 2.f;
    if (is_activated) {
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(vector_icons::kBackArrowIcon, kArrowSize,
                                kArrowColorAfterActivated),
          static_cast<int>(arrow_x), static_cast<int>(arrow_y));
    } else {
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(vector_icons::kBackArrowIcon, kArrowSize,
                                kArrowColorBeforeActivated),
          static_cast<int>(arrow_x), static_cast<int>(arrow_y));
    }
  }

  float complete_progress_ = 0.f;
  BackGestureAffordance::State state_ = BackGestureAffordance::State::DRAGGING;
  float x_offset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AffordanceView);
};

// Get the bounds of the affordance widget, which is outside of the left edge of
// the display.
gfx::Rect GetWidgetBounds(const gfx::Point& location) {
  gfx::Rect widget_bounds(
      gfx::Rect(2 * kMaxBurstRippleRadius, 2 * kMaxBurstRippleRadius));
  gfx::Point origin;
  origin.set_x(-kMaxBurstRippleRadius - kBackgroundRadius);
  int origin_y =
      location.y() - kDistanceFromArrowToTouchPoint - kMaxBurstRippleRadius;
  if (origin_y < 0) {
    origin_y =
        location.y() + kDistanceFromArrowToTouchPoint - kMaxBurstRippleRadius;
  }
  origin.set_y(origin_y);
  widget_bounds.set_origin(origin);
  return widget_bounds;
}

}  // namespace

BackGestureAffordance::BackGestureAffordance(const gfx::Point& location) {
  CreateAffordanceWidget(location);
}

BackGestureAffordance::~BackGestureAffordance() {}

void BackGestureAffordance::Update(int x_drag_amount,
                                   int y_drag_amount,
                                   bool during_reverse_dragging) {
  DCHECK_EQ(State::DRAGGING, state_);

  // Since affordance is put outside of the display, add the distance from its
  // center point to the left edge of the display to be the actual drag
  // distance.
  x_drag_amount_ = x_drag_amount + kBackgroundRadius;

  float y_progress = y_drag_amount / kDistanceForFullYProgress;
  y_drag_progress_ = std::min(1.0f, std::max(-1.0f, y_progress));

  during_reverse_dragging_ = during_reverse_dragging;

  UpdateTransform();
  SchedulePaint();
}

void BackGestureAffordance::Abort() {
  DCHECK_EQ(State::DRAGGING, state_);

  state_ = State::ABORTING;
  started_reverse_ = false;
  x_drag_amount_ = current_offset_;
  animation_ = std::make_unique<gfx::LinearAnimation>(
      GetAffordanceProgress() * kAbortAnimationTimeout,
      gfx::LinearAnimation::kDefaultFrameRate, this);
  animation_->Start();
}

void BackGestureAffordance::Complete() {
  DCHECK_EQ(State::DRAGGING, state_);
  state_ = State::COMPLETING;

  animation_ = std::make_unique<gfx::LinearAnimation>(
      kCompleteAnimationTimeout, gfx::LinearAnimation::kDefaultFrameRate, this);
  animation_->Start();
}

bool BackGestureAffordance::IsActivated() const {
  return current_offset_ >= kDistanceForFullRadius;
}

void BackGestureAffordance::CreateAffordanceWidget(const gfx::Point& location) {
  affordance_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "BackGestureAffordance";
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.parent = window_util::GetRootWindowAt(location)->GetChildById(
      kShellWindowId_AlwaysOnTopContainer);
  affordance_widget_->Init(std::move(params));
  affordance_widget_->SetContentsView(new AffordanceView());
  affordance_widget_->SetBounds(GetWidgetBounds(location));
  affordance_widget_->Show();
  affordance_widget_->SetOpacity(1.f);
}

void BackGestureAffordance::UpdateTransform() {
  float offset = 0.f;
  if (started_reverse_) {
    offset = x_drag_amount_ -
             (x_drag_amount_on_start_reverse_ - offset_on_start_reverse_);
    // Reset the previous started reverse drag if back to its started position.
    if (!during_reverse_dragging_ &&
        x_drag_amount_ >= x_drag_amount_on_start_reverse_) {
      started_reverse_ = false;
    }
  } else {
    if (x_drag_amount_ <= kDistanceForFullRadius) {
      offset = GetAffordanceProgress() * kDistanceForFullRadius;
    } else {
      if (during_reverse_dragging_) {
        started_reverse_ = true;
        offset_on_start_reverse_ = current_offset_;
        x_drag_amount_on_start_reverse_ = x_drag_amount_;
      }
      const float factor = (kDistanceForMaxRadius - kDistanceForFullRadius) /
                           (kDragDistanceForMaxRadius - kDistanceForFullRadius);
      offset = (kDistanceForFullRadius - kDistanceForFullRadius * factor) +
               factor * x_drag_amount_;
    }
  }
  offset = std::fmin(kDistanceForMaxRadius, std::fmax(0, offset));
  current_offset_ = offset;

  float y_offset = kMaxYMovement * y_drag_progress_;
  gfx::Transform transform;
  transform.Translate(offset, y_offset);
  affordance_widget_->GetContentsView()->SetTransform(transform);
}

void BackGestureAffordance::SchedulePaint() {
  static_cast<AffordanceView*>(affordance_widget_->GetContentsView())
      ->Paint(complete_progress_, state_, current_offset_);
}

void BackGestureAffordance::SetAbortProgress(float progress) {
  DCHECK_EQ(State::ABORTING, state_);
  DCHECK_LE(0.f, progress);
  DCHECK_GE(1.f, progress);

  if (abort_progress_ == progress)
    return;
  abort_progress_ = progress;

  UpdateTransform();
  SchedulePaint();
}

void BackGestureAffordance::SetCompleteProgress(float progress) {
  DCHECK_EQ(State::COMPLETING, state_);
  DCHECK_LE(0.f, progress);
  DCHECK_GE(1.f, progress);

  if (complete_progress_ == progress)
    return;
  complete_progress_ = progress;

  affordance_widget_->GetContentsView()->layer()->SetOpacity(
      gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                 1 - complete_progress_));

  SchedulePaint();
}

float BackGestureAffordance::GetAffordanceProgress() const {
  return (x_drag_amount_ / kDistanceForFullRadius) *
         (1 - gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                         abort_progress_));
}

void BackGestureAffordance::AnimationEnded(const gfx::Animation* animation) {}

void BackGestureAffordance::AnimationProgressed(
    const gfx::Animation* animation) {
  switch (state_) {
    case State::DRAGGING:
      NOTREACHED();
      break;
    case State::ABORTING:
      SetAbortProgress(animation->GetCurrentValue());
      break;
    case State::COMPLETING:
      SetCompleteProgress(animation->GetCurrentValue());
      break;
  }
}

void BackGestureAffordance::AnimationCanceled(const gfx::Animation* animation) {
  NOTREACHED();
}

}  // namespace ash
