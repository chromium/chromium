// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/expand_arrow_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

// The width of this view.
constexpr int kTileWidth = 60;

// The arrow dimension of expand arrow.
constexpr int kArrowDimension = 12;

constexpr int kInkDropRadius = 18;
constexpr int kCircleRadius = 18;

// The y position of circle center in closed, peeking and fullscreen state.
constexpr int kCircleCenterClosedY = 18;
constexpr int kCircleCenterPeekingY = 42;
constexpr int kCircleCenterFullscreenY = 8;

// The arrow's y position in peeking and fullscreen state.
constexpr int kArrowClosedY = 12;
constexpr int kArrowPeekingY = 36;
constexpr int kArrowFullscreenY = 2;

// The stroke width of the arrow.
constexpr int kExpandArrowStrokeWidth = 2;

// The three points of arrow in peeking and fullscreen state.
constexpr size_t kPointCount = 3;
constexpr gfx::PointF kPeekingPoints[] = {
    gfx::PointF(0, kArrowDimension / 4 * 3),
    gfx::PointF(kArrowDimension / 2, kArrowDimension / 4),
    gfx::PointF(kArrowDimension, kArrowDimension / 4 * 3)};
constexpr gfx::PointF kFullscreenPoints[] = {
    gfx::PointF(0, kArrowDimension / 2),
    gfx::PointF(kArrowDimension / 2, kArrowDimension / 2),
    gfx::PointF(kArrowDimension, kArrowDimension / 2)};

// Arrow vertical transiton animation related constants.
constexpr int kTotalArrowYOffset = 24;
constexpr int kPulseMinRadius = 2;
constexpr int kPulseMaxRadius = 30;
constexpr float kPulseMinOpacity = 0.f;
constexpr float kPulseMaxOpacity = 0.3f;
constexpr int kAnimationInitialWaitTimeInSec = 3;
constexpr int kAnimationIntervalInSec = 10;
constexpr auto kCycleDuration = base::TimeDelta::FromMilliseconds(1000);
constexpr auto kCycleInterval = base::TimeDelta::FromMilliseconds(500);

constexpr SkColor kFocusRingColor = gfx::kGoogleBlue300;
constexpr int kFocusRingWidth = 2;

// THe bounds for the tap target of the expand arrow button.
constexpr int kTapTargetWidth = 156;
constexpr int kTapTargetHeight = 72;

float GetCircleCenterYForAppListProgress(float progress) {
  if (progress <= 1) {
    return gfx::Tween::FloatValueBetween(progress, kCircleCenterClosedY,
                                         kCircleCenterPeekingY);
  }
  return gfx::Tween::FloatValueBetween(std::min(1.0f, progress - 1),
                                       kCircleCenterPeekingY,
                                       kCircleCenterFullscreenY);
}

float GetArrowYForAppListProgress(float progress) {
  if (progress <= 1) {
    return gfx::Tween::FloatValueBetween(progress, kArrowClosedY,
                                         kArrowPeekingY);
  }
  return gfx::Tween::FloatValueBetween(std::min(1.0f, progress - 1),
                                       kArrowPeekingY, kArrowFullscreenY);
}

// Returns the location of the circle, relative to the view's local bounds.
gfx::Rect GetCircleBounds() {
  const gfx::Point circle_center(kTileWidth / 2, kCircleCenterPeekingY);
  const gfx::Rect circle_bounds(
      circle_center - gfx::Vector2d(kCircleRadius, kCircleRadius),
      gfx::Size(2 * kCircleRadius, 2 * kCircleRadius));
  return circle_bounds;
}

class ExpandArrowHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  ExpandArrowHighlightPathGenerator() = default;

  ExpandArrowHighlightPathGenerator(const ExpandArrowHighlightPathGenerator&) =
      delete;
  ExpandArrowHighlightPathGenerator& operator=(
      const ExpandArrowHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  base::Optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    return gfx::RRectF(gfx::RectF(GetCircleBounds()), kInkDropRadius);
  }
};

}  // namespace

ExpandArrowView::ExpandArrowView(ContentsView* contents_view,
                                 AppListView* app_list_view)
    : views::Button(this),
      contents_view_(contents_view),
      app_list_view_(app_list_view) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // ExpandArrowView draws its own focus, removing FocusRing prevents double
  // focus.
  // TODO(pbos): Replace ::OnPaint focus painting with FocusRing +
  // HighlightPathGenerator usage.
  SetInstallFocusRingOnFocus(false);
  SetInkDropMode(InkDropMode::ON);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<ExpandArrowHighlightPathGenerator>());

  SetAccessibleName(l10n_util::GetStringUTF16(IDS_APP_LIST_EXPAND_BUTTON));

  animation_ = std::make_unique<gfx::SlideAnimation>(this);
  animation_->SetTweenType(gfx::Tween::LINEAR);
  animation_->SetSlideDuration(kCycleDuration * 2 + kCycleInterval);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

ExpandArrowView::~ExpandArrowView() = default;

void ExpandArrowView::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::PointF circle_center(kTileWidth / 2, kCircleCenterPeekingY);
  gfx::PointF arrow_origin((kTileWidth - kArrowDimension) / 2, kArrowPeekingY);
  gfx::PointF arrow_points[kPointCount];
  for (size_t i = 0; i < kPointCount; ++i)
    arrow_points[i] = kPeekingPoints[i];
  SkColor circle_color =
      AppListColorProvider::Get()->GetExpandArrowIconBackgroundColor();
  const float progress = app_list_view_->GetAppListTransitionProgress(
      AppListView::kProgressFlagNone);
  circle_center.set_y(GetCircleCenterYForAppListProgress(progress));
  arrow_origin.set_y(GetArrowYForAppListProgress(progress));
  // If transition progress is between peeking and fullscreen state, change
  // the shape of the arrow and the opacity of the circle in addition to
  // changing the circle and arrow position.
  if (progress > 1) {
    const float peeking_to_full_progress = progress - 1;
    for (size_t i = 0; i < kPointCount; ++i) {
      arrow_points[i].set_y(gfx::Tween::FloatValueBetween(
          peeking_to_full_progress, kPeekingPoints[i].y(),
          kFullscreenPoints[i].y()));
    }
    circle_color = gfx::Tween::ColorValueBetween(
        peeking_to_full_progress, circle_color, SK_ColorTRANSPARENT);
  }

  if (animation_->is_animating() && progress <= 1) {
    // If app list is peeking state or below peeking state, the arrow should
    // keep runing transition animation.
    arrow_origin.Offset(0, arrow_y_offset_);
  }

  // Draw a circle.
  cc::PaintFlags circle_flags;
  circle_flags.setAntiAlias(true);
  circle_flags.setColor(circle_color);
  circle_flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(circle_center, kCircleRadius, circle_flags);

  if (HasFocus()) {
    cc::PaintFlags focus_ring_flags;
    focus_ring_flags.setAntiAlias(true);
    focus_ring_flags.setColor(kFocusRingColor);
    focus_ring_flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
    focus_ring_flags.setStrokeWidth(kFocusRingWidth);

    // Creates a focus ring with 1px wider radius to create a border.
    canvas->DrawCircle(circle_center, kCircleRadius + 1, focus_ring_flags);
  }

  if (animation_->is_animating() && progress <= 1) {
    // Draw a pulse that expands around the circle.
    cc::PaintFlags pulse_flags;
    pulse_flags.setStyle(cc::PaintFlags::kStroke_Style);
    pulse_flags.setColor(
        SkColorSetA(AppListColorProvider::Get()->GetExpandArrowIconBaseColor(),
                    static_cast<U8CPU>(255 * pulse_opacity_)));
    pulse_flags.setAntiAlias(true);
    canvas->DrawCircle(circle_center, pulse_radius_, pulse_flags);
  }

  // Add a clip path so that arrow will only be shown within the circular
  // highlight area.
  SkPath arrow_mask_path;
  arrow_mask_path.addCircle(circle_center.x(), circle_center.y(),
                            kCircleRadius);
  canvas->ClipPath(arrow_mask_path, true);

  // Draw an arrow. (It becomes a horizontal line in fullscreen state.)
  for (auto& point : arrow_points)
    point.Offset(arrow_origin.x(), arrow_origin.y());

  cc::PaintFlags arrow_flags;
  arrow_flags.setAntiAlias(true);

  arrow_flags.setColor(
      AppListColorProvider::Get()->GetExpandArrowIconBaseColor());
  arrow_flags.setStrokeWidth(kExpandArrowStrokeWidth);
  arrow_flags.setStrokeCap(cc::PaintFlags::Cap::kRound_Cap);
  arrow_flags.setStrokeJoin(cc::PaintFlags::Join::kRound_Join);
  arrow_flags.setStyle(cc::PaintFlags::kStroke_Style);

  SkPath arrow_path;
  arrow_path.moveTo(arrow_points[0].x(), arrow_points[0].y());
  for (size_t i = 1; i < kPointCount; ++i)
    arrow_path.lineTo(arrow_points[i].x(), arrow_points[i].y());
  canvas->DrawPath(arrow_path, arrow_flags);
}

void ExpandArrowView::ButtonPressed(views::Button* /*sender*/,
                                    const ui::Event& /*event*/) {
  button_pressed_ = true;
  ResetHintingAnimation();
  TransitToFullscreenAllAppsState();
  GetInkDrop()->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
}

gfx::Size ExpandArrowView::CalculatePreferredSize() const {
  return gfx::Size(kTileWidth,
                   AppListConfig::instance().expand_arrow_tile_height());
}

bool ExpandArrowView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_RETURN)
    return false;
  TransitToFullscreenAllAppsState();
  return true;
}

void ExpandArrowView::OnFocus() {
  SchedulePaint();
  Button::OnFocus();
}

void ExpandArrowView::OnBlur() {
  SchedulePaint();
  Button::OnBlur();
}

const char* ExpandArrowView::GetClassName() const {
  return "ExpandArrowView";
}

std::unique_ptr<views::InkDrop> ExpandArrowView::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      Button::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetAutoHighlightMode(views::InkDropImpl::AutoHighlightMode::NONE);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> ExpandArrowView::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(GetCircleBounds()),
      GetInkDropCenterBasedOnLastEvent(),
      AppListColorProvider::Get()->GetExpandArrowInkDropBaseColor(), 1.0f);
}

void ExpandArrowView::AnimationProgressed(const gfx::Animation* animation) {
  // There are two cycles in one animation.
  constexpr auto kAnimationDuration = kCycleDuration * 2 + kCycleInterval;
  constexpr auto kFirstCycleEndTime = kCycleDuration;
  constexpr auto kIntervalEndTime = kCycleDuration + kCycleInterval;
  constexpr auto kSecondCycleEndTime = kCycleDuration * 2 + kCycleInterval;
  base::TimeDelta time = animation->GetCurrentValue() * kAnimationDuration;

  if (time > kFirstCycleEndTime && time <= kIntervalEndTime) {
    // There's no animation in the interval between cycles.
    return;
  }
  if (time > kIntervalEndTime && time <= kSecondCycleEndTime) {
    // Convert to time in one single cycle.
    time -= kIntervalEndTime;
  }

  // Update pulse opacity.
  constexpr auto kPulseOpacityShowBeginTime =
      base::TimeDelta::FromMilliseconds(100);
  constexpr auto kPulseOpacityShowEndTime =
      base::TimeDelta::FromMilliseconds(200);
  constexpr auto kPulseOpacityHideBeginTime =
      base::TimeDelta::FromMilliseconds(800);
  constexpr auto kPulseOpacityHideEndTime =
      base::TimeDelta::FromMilliseconds(1000);
  if (time > kPulseOpacityShowBeginTime && time <= kPulseOpacityShowEndTime) {
    pulse_opacity_ =
        kPulseMinOpacity +
        (kPulseMaxOpacity - kPulseMinOpacity) *
            (time - kPulseOpacityShowBeginTime) /
            (kPulseOpacityShowEndTime - kPulseOpacityShowBeginTime);
  } else if (time > kPulseOpacityHideBeginTime &&
             time <= kPulseOpacityHideEndTime) {
    pulse_opacity_ =
        kPulseMaxOpacity -
        (kPulseMaxOpacity - kPulseMinOpacity) *
            (time - kPulseOpacityHideBeginTime) /
            (kPulseOpacityHideEndTime - kPulseOpacityHideBeginTime);
  }

  // Update pulse radius.
  pulse_radius_ =
      base::ClampRound((kPulseMaxRadius - kPulseMinRadius) *
                       gfx::Tween::CalculateValue(gfx::Tween::EASE_IN_OUT,
                                                  time / kCycleDuration));

  // Update y position offset of the arrow.
  constexpr auto kArrowMoveOutBeginTime =
      base::TimeDelta::FromMilliseconds(100);
  constexpr auto kArrowMoveOutEndTime = base::TimeDelta::FromMilliseconds(500);
  constexpr auto kArrowMoveInBeginTime = base::TimeDelta::FromMilliseconds(500);
  constexpr auto kArrowMoveInEndTime = base::TimeDelta::FromMilliseconds(900);
  if (time > kArrowMoveOutBeginTime && time <= kArrowMoveOutEndTime) {
    const double progress = (time - kArrowMoveOutBeginTime) /
                            (kArrowMoveOutEndTime - kArrowMoveOutBeginTime);
    arrow_y_offset_ = base::ClampRound(
        -kTotalArrowYOffset *
        gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, progress));
  } else if (time > kArrowMoveInBeginTime && time <= kArrowMoveInEndTime) {
    const double progress = (time - kArrowMoveInBeginTime) /
                            (kArrowMoveInEndTime - kArrowMoveInBeginTime);
    arrow_y_offset_ = base::ClampRound(
        kTotalArrowYOffset *
        (1 - gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, progress)));
  }

  // Apply updates.
  SchedulePaint();
}

void ExpandArrowView::AnimationEnded(const gfx::Animation* /*animation*/) {
  ResetHintingAnimation();
  // Only reschedule hinting animation if app list is not fullscreen. Once the
  // user has made the app_list fullscreen, a hint to do so is no longer needed
  if (!app_list_view_->is_fullscreen())
    ScheduleHintingAnimation(false);
}

void ExpandArrowView::TransitToFullscreenAllAppsState() {
  UMA_HISTOGRAM_ENUMERATION(kPageOpenedHistogram, AppListState::kStateApps,
                            AppListState::kStateLast);
  UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram, kExpandArrow,
                            kMaxPeekingToFullscreen);
  contents_view_->SetActiveState(AppListState::kStateApps);
  app_list_view_->SetState(AppListViewState::kFullscreenAllApps);
}

void ExpandArrowView::MaybeEnableHintingAnimation(bool enabled) {
  button_pressed_ = false;
  ResetHintingAnimation();
  // When side shelf or tablet mode is enabled, the peeking launcher won't be
  // shown, so the hint animation is unnecessary. Also, do not run the animation
  // during test since we are not testing the animation and it might cause msan
  // crash when spoken feedback is enabled (See https://crbug.com/926038).
  if (enabled && !app_list_view_->is_side_shelf() &&
      !app_list_view_->is_tablet_mode() &&
      !AppListView::ShortAnimationsForTesting()) {
    ScheduleHintingAnimation(true);
  } else {
    hinting_animation_timer_.Stop();
  }
}

float ExpandArrowView::CalculateOffsetFromCurrentAppListProgress(
    double progress) const {
  const float current_progress = app_list_view_->GetAppListTransitionProgress(
      AppListView::kProgressFlagNone);
  return GetCircleCenterYForAppListProgress(progress) -
         GetCircleCenterYForAppListProgress(current_progress);
}

void ExpandArrowView::ScheduleHintingAnimation(bool is_first_time) {
  int delay_in_sec = kAnimationIntervalInSec;
  if (is_first_time)
    delay_in_sec = kAnimationInitialWaitTimeInSec;
  hinting_animation_timer_.Start(FROM_HERE,
                                 base::TimeDelta::FromSeconds(delay_in_sec),
                                 this, &ExpandArrowView::StartHintingAnimation);
}

void ExpandArrowView::StartHintingAnimation() {
  if (!button_pressed_)
    animation_->Show();
}

void ExpandArrowView::ResetHintingAnimation() {
  pulse_opacity_ = kPulseMinOpacity;
  pulse_radius_ = kPulseMinRadius;
  animation_->Reset();
  Layout();
}

bool ExpandArrowView::DoesIntersectRect(const views::View* target,
                                        const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect button_bounds = GetLocalBounds();
  // Increase clickable area for the button from
  // (kTileWidth x height) to
  // (kTapTargetWidth x kTapTargetHeight).
  const int horizontal_padding = (kTapTargetWidth - button_bounds.width()) / 2;
  const int vertical_padding = (kTapTargetHeight - button_bounds.height()) / 2;
  button_bounds.Inset(-horizontal_padding, -vertical_padding);
  return button_bounds.Intersects(rect);
}

}  // namespace ash
