// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_affordance.h"

#include <algorithm>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/wm/gestures/back_gesture/back_gesture_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/window_util.h"
#include "base/i18n/rtl.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Distance from the arrow to the drag touch point. The arrow is usually
// above the touch point but will be put below the touch point if the affordance
// is outside of the display.
constexpr int kDistanceFromArrowToTouchPoint = 64;

constexpr int kArrowSize = 20;
constexpr int kBackgroundRadius = 20;

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

constexpr base::TimeDelta kAbortAnimationTimeout = base::Milliseconds(300);
constexpr base::TimeDelta kCompleteAnimationTimeout = base::Milliseconds(200);

// Y-axis drag distance to achieve full y drag progress.
constexpr float kDistanceForFullYProgress = 80.f;

// Distance of the affordance that beyond the left of display or splitview
// divider.
constexpr int kDistanceBeyondLeftOrSplitvieDivider =
    kMaxBurstRippleRadius + kBackgroundRadius;

// Maximium y-axis movement of the affordance. Note, the affordance can move
// both up and down.
constexpr float kMaxYMovement = 8.f;

class AffordanceView : public views::View {
 public:
  AffordanceView() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }
  AffordanceView(AffordanceView&) = delete;
  AffordanceView& operator=(AffordanceView&) = delete;
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
    ripple_flags.setColor(ColorUtil::GetSecondToneColor(
        GetColorProvider()->GetColor(kColorAshControlBackgroundColorActive)));

    float ripple_radius = 0.f;
    if (state_ == BackGestureAffordance::State::COMPLETING) {
      const float burst_progress = gfx::Tween::CalculateValue(
          gfx::Tween::FAST_OUT_SLOW_IN, complete_progress_);
      ripple_radius =
          kMaxRippleRadius +
          burst_progress * (kMaxBurstRippleRadius - kMaxRippleRadius);
    } else if (x_offset_ >= kDistanceForFullRadius) {
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

    const bool is_activated =
        x_offset_ >= kDistanceForFullRadius ||
        state_ == BackGestureAffordance::State::COMPLETING;

    // Draw highlight border circles.
    DrawCircleHighlightBorder(this, canvas, center_point, kBackgroundRadius);

    // Draw the arrow background circle.
    cc::PaintFlags bg_flags;
    bg_flags.setAntiAlias(true);
    bg_flags.setStyle(cc::PaintFlags::kFill_Style);

    const auto* color_provider = GetColorProvider();
    bg_flags.setColor(color_provider->GetColor(
        is_activated ? kColorAshControlBackgroundColorActive
                     : kColorAshShieldAndBaseOpaque));
    canvas->DrawCircle(center_point, kBackgroundRadius, bg_flags);

    // Draw the arrow.
    const float arrow_x = center_point.x() - kArrowSize / 2.f;
    const float arrow_y = center_point.y() - kArrowSize / 2.f;
    const bool is_rtl = base::i18n::IsRTL();
    if (is_activated) {
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(
              is_rtl ? vector_icons::kForwardArrowIcon
                     : vector_icons::kBackArrowIcon,
              kArrowSize,
              color_provider->GetColor(kColorAshButtonIconColorPrimary)),
          static_cast<int>(arrow_x), static_cast<int>(arrow_y));
    } else {
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(
              is_rtl ? vector_icons::kForwardArrowIcon
                     : vector_icons::kBackArrowIcon,
              kArrowSize, color_provider->GetColor(kColorAshButtonIconColor)),
          static_cast<int>(arrow_x), static_cast<int>(arrow_y));
    }
  }

  float complete_progress_ = 0.f;
  BackGestureAffordance::State state_ = BackGestureAffordance::State::DRAGGING;
  float x_offset_ = 0;
};

gfx::Rect GetSplitViewDividerBoundsInScreen(const gfx::Point& location) {
  auto* split_view_controller =
      SplitViewController::Get(window_util::GetRootWindowAt(location));
  if (!split_view_controller->InTabletSplitViewMode())
    return gfx::Rect();

  return split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
      /*is_dragging=*/false);
}

// Return true if |origin_y| is above the bottom of the splitview divider while
// in portrait screen orientation.
bool AboveBottomOfSplitViewDivider(const gfx::Point& location, int origin_y) {
  auto* split_view_controller =
      SplitViewController::Get(window_util::GetRootWindowAt(location));
  if (!split_view_controller->InTabletSplitViewMode() ||
      IsCurrentScreenOrientationLandscape()) {
    return false;
  }

  const gfx::Rect bounds_of_bottom_snapped_window =
      split_view_controller->GetSnappedWindowBoundsInScreen(
          IsCurrentScreenOrientationPrimary() ? SnapPosition::kSecondary
                                              : SnapPosition::kPrimary,
          /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
          /*account_for_divider_width=*/true);
  return bounds_of_bottom_snapped_window.Contains(location) &&
         origin_y < GetSplitViewDividerBoundsInScreen(location).bottom();
}

gfx::Rect GetAffordanceBounds(const gfx::Point& location,
                              bool dragged_from_splitview_divider) {
  gfx::Rect bounds(
      gfx::Rect(2 * kMaxBurstRippleRadius, 2 * kMaxBurstRippleRadius));

  gfx::Point origin;
  // X origin of the affordance is always beyond the left of the screen. We'll
  // apply translation to the affordance to put it in the right place during
  // dragging.
  const gfx::Rect work_area = display::Screen::GetScreen()
                                  ->GetDisplayNearestPoint(location)
                                  .work_area();
  origin.set_x(work_area.x() - kDistanceBeyondLeftOrSplitvieDivider);

  int origin_y =
      location.y() - kDistanceFromArrowToTouchPoint - kMaxBurstRippleRadius;
  // Put the affordance below the start |location| if |origin_y| exceeds the
  // top of the display or bottom of the splitview divider.
  if (origin_y < 0 || AboveBottomOfSplitViewDivider(location, origin_y)) {
    origin_y =
        location.y() + kDistanceFromArrowToTouchPoint - kMaxBurstRippleRadius;
  }
  origin.set_y(origin_y);
  bounds.set_origin(origin);
  return bounds;
}

// Returns the mirrored location of |location| if we're in rtl setting. If
// |dragged_from_splitview_divider| is true,  it will return the mirrored
// location against the center x position of the divider bar, otherwise, it will
// return the mirrored location against the center x position of the screen.
gfx::Point ToMirrorLocationIfRTL(const gfx::Point& location,
                                 bool dragged_from_splitview_divider) {
  if (!base::i18n::IsRTL())
    return location;

  const gfx::Rect work_area = display::Screen::GetScreen()
                                  ->GetDisplayNearestPoint(location)
                                  .work_area();
  if (!dragged_from_splitview_divider) {
    return gfx::Point(work_area.right() + work_area.x() - location.x(),
                      location.y());
  }

  const gfx::Rect divider_bounds = GetSplitViewDividerBoundsInScreen(location);
  return gfx::Point(2 * divider_bounds.CenterPoint().x() - location.x(),
                    location.y());
}

}  // namespace

BackGestureAffordance::BackGestureAffordance(
    const gfx::Point& location,
    bool dragged_from_splitview_divider)
    : dragged_from_splitview_divider_(dragged_from_splitview_divider) {
  CreateAffordanceWidget(
      ToMirrorLocationIfRTL(location, dragged_from_splitview_divider));
}

BackGestureAffordance::~BackGestureAffordance() {}

void BackGestureAffordance::Update(int x_drag_amount,
                                   int y_drag_amount,
                                   bool during_reverse_dragging) {
  DCHECK_EQ(State::DRAGGING, state_);

  // Since affordance is put outside of the display, add the distance from its
  // center point to the left edge of the display to be the actual drag
  // distance.
  x_drag_amount_ = (base::i18n::IsRTL() ? -x_drag_amount : x_drag_amount) +
                   kBackgroundRadius;

  float y_progress = y_drag_amount / kDistanceForFullYProgress;
  y_drag_progress_ = std::clamp(y_progress, -1.0f, 1.0f);

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
  CHECK_EQ(State::DRAGGING, state_);
  state_ = State::COMPLETING;

  animation_ = std::make_unique<gfx::LinearAnimation>(
      kCompleteAnimationTimeout, gfx::LinearAnimation::kDefaultFrameRate, this);
  animation_->Start();
}

bool BackGestureAffordance::IsActivated() const {
  return current_offset_ >= kDistanceForFullRadius ||
         state_ == State::COMPLETING;
}

void BackGestureAffordance::CreateAffordanceWidget(const gfx::Point& location) {
  affordance_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = true;
  params.name = "BackGestureAffordance";
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.parent = window_util::GetRootWindowAt(location)->GetChildById(
      kShellWindowId_OverlayContainer);

  affordance_widget_->Init(std::move(params));
  affordance_widget_->SetContentsView(std::make_unique<AffordanceView>());
  // We've got our own custom show/hide animation, so the default is unneeded.
  ::wm::SetWindowVisibilityAnimationTransition(
      affordance_widget_->GetNativeWindow(), ::wm::ANIMATE_NONE);

  const gfx::Rect widget_bounds =
      GetAffordanceBounds(location, dragged_from_splitview_divider_);
  affordance_widget_->SetBounds(widget_bounds);
  if (dragged_from_splitview_divider_) {
    // Clip the affordance to make sure it will only be visible inside the
    // snapped window's bounds. Note, |clip_bounds| is the area that the
    // affordance will be visible, and it is based on the layer's coordinate.
    gfx::Rect clip_bounds;
    const gfx::Rect divider_bounds =
        GetSplitViewDividerBoundsInScreen(location);
    const gfx::Rect work_area = display::Screen::GetScreen()
                                    ->GetDisplayNearestPoint(location)
                                    .work_area();
    if (base::i18n::IsRTL()) {
      clip_bounds = gfx::Rect(divider_bounds.x() - kDistanceForMaxRadius -
                                  kMaxBurstRippleRadius - widget_bounds.x(),
                              0, kDistanceForMaxRadius + kMaxBurstRippleRadius,
                              widget_bounds.height());
    } else {
      clip_bounds = gfx::Rect(divider_bounds.right() - work_area.x() +
                                  kDistanceBeyondLeftOrSplitvieDivider,
                              0, kDistanceForMaxRadius + kMaxBurstRippleRadius,
                              widget_bounds.height());
    }
    affordance_widget_->GetLayer()->SetClipRect(clip_bounds);
  }
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

  // Adjusting the affordance offset based on different configurations (e.g.,
  // drag from split view divider bar or rtl language) so that affordance can
  // remain under or above the finger.
  const gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(affordance_widget_->GetNativeWindow())
          .work_area();
  if (dragged_from_splitview_divider_) {
    auto* split_view_controller = SplitViewController::Get(
        affordance_widget_->GetNativeWindow()->GetRootWindow());
    const gfx::Rect divider_bounds =
        split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
            /*is_dragging=*/false);
    if (base::i18n::IsRTL()) {
      offset = divider_bounds.right() - work_area.x() - offset +
               kMaxBurstRippleRadius;
    } else {
      offset += divider_bounds.right() - work_area.x();
    }
  } else if (base::i18n::IsRTL()) {
    offset = work_area.width() - offset + kMaxBurstRippleRadius;
  }

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

void BackGestureAffordance::AnimationEnded(const gfx::Animation* animation) {
  affordance_widget_->Hide();
}

void BackGestureAffordance::AnimationProgressed(
    const gfx::Animation* animation) {
  switch (state_) {
    case State::DRAGGING:
      NOTREACHED();
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
