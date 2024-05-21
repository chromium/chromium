// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge.h"

#include "ash/controls/contextual_tooltip.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/gestures/back_gesture/back_gesture_util.h"
#include "base/i18n/rtl.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Width of the contextual nudge.
constexpr int kBackgroundWidth = 320;

// Radius of the circle in the middle of the contextual nudge.
constexpr int kCircleRadius = 20;

// Width of the circle that inside the screen at the beginning.
constexpr int kCircleInsideScreenWidth = 12;

// Padding between the circle and the label.
constexpr int kPaddingBetweenCircleAndLabel = 8;

// Line height of the label.
constexpr int kLabelLineHeight = 18;

// Corner radius for the label's background.
constexpr int kLabelCornerRadius = 16;

// Top and bottom inset of the label.
constexpr int kLabelTopBottomInset = 6;

// Duration of the pause before sliding in to show the nudge.
constexpr base::TimeDelta kPauseBeforeShowAnimationDuration = base::Seconds(10);

// Duration for the animation to show the nudge.
constexpr base::TimeDelta kNudgeShowAnimationDuration = base::Milliseconds(600);

// Duration for the animation to hide the nudge.
constexpr base::TimeDelta kNudgeHideAnimationDuration = base::Milliseconds(400);

// Duration for the animation to fade out the suggestion label and circle when
// the back nudge showing animation is interrupted and should be dismissed.
constexpr base::TimeDelta kSuggestionDismissDuration = base::Milliseconds(100);

// Duration for the animation of the suggestion part of the nudge.
constexpr base::TimeDelta kSuggestionBounceAnimationDuration =
    base::Milliseconds(600);

// Repeat bouncing times of the suggestion animation.
constexpr int kSuggestionAnimationRepeatTimes = 4;

std::unique_ptr<views::Widget> CreateWidget() {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.name = "BackGestureContextualNudge";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_OverlayContainer);
  widget->Init(std::move(params));

  // TODO(crbug.com/40100889): Get the bounds of the display that should show
  // the nudge, which may based on the conditions to show the nudge.
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  gfx::Rect widget_bounds;
  if (base::i18n::IsRTL()) {
    widget_bounds = gfx::Rect(display_bounds.right(), display_bounds.y(),
                              kBackgroundWidth, display_bounds.height());
  } else {
    widget_bounds =
        gfx::Rect(display_bounds.x() - kBackgroundWidth, display_bounds.y(),
                  kBackgroundWidth, display_bounds.height());
  }
  widget->SetBounds(widget_bounds);
  return widget;
}

}  // namespace

class BackGestureContextualNudge::ContextualNudgeView
    : public views::View,
      public ui::ImplicitAnimationObserver {
  METADATA_HEADER(ContextualNudgeView, views::View)

 public:
  explicit ContextualNudgeView(base::OnceCallback<void(bool)> callback)
      : callback_(std::move(callback)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    suggestion_view_ = AddChildView(std::make_unique<SuggestionView>(this));
    show_timer_.Start(
        FROM_HERE, kPauseBeforeShowAnimationDuration, this,
        &ContextualNudgeView::ScheduleOffScreenToStartPositionAnimation);
  }
  ContextualNudgeView(const ContextualNudgeView&) = delete;
  ContextualNudgeView& operator=(const ContextualNudgeView&) = delete;

  ~ContextualNudgeView() override { StopObservingImplicitAnimations(); }

  // Cancel in-waiting animation or in-progress animation.
  void CancelAnimationOrFadeOutToHide() {
    if (animation_stage_ == AnimationStage::kWaitingCancelled ||
        animation_stage_ == AnimationStage::kFadingOut) {
      return;
    }

    if (animation_stage_ == AnimationStage::kWaiting) {
      // Cancel the animation if it's waiting to be shown.
      animation_stage_ = AnimationStage::kWaitingCancelled;
      DCHECK(show_timer_.IsRunning());
      show_timer_.AbandonAndStop();
      std::move(callback_).Run(/*animation_completed=*/false);
    } else if (animation_stage_ == AnimationStage::kSlidingIn ||
               animation_stage_ == AnimationStage::kBouncing ||
               animation_stage_ == AnimationStage::kSlidingOut) {
      // Cancel previous animations and fade out the widget if it's animating.
      layer()->GetAnimator()->AbortAllAnimations();
      suggestion_view_->layer()->GetAnimator()->AbortAllAnimations();

      animation_stage_ = AnimationStage::kFadingOut;
      suggestion_view_->FadeOutForDismiss();
    }
  }

  void SetNudgeShownForTesting() { SetNudgeCountsAsShown(); }

  bool count_as_shown() const { return count_as_shown_; }

 private:
  enum class AnimationStage {
    kWaiting,     // Animation hasn't been started.
    kSlidingIn,   // Sliding in animation to show the affordance and label.
    kBouncing,    // Bouncing the affordance and label animation.
    kSlidingOut,  // Sliding out animation to hide the affordance and label.
    kWaitingCancelled,  // The in-waiting animation is cancelled.
    kFadingOut,  // Previous in-progress animations are cancelled, fading out
                 // the affordance and label.
  };

  // Used to show the suggestion information of the nudge, which includes the
  // affordance and a label.
  class SuggestionView : public views::View,
                         public ui::ImplicitAnimationObserver {
    METADATA_HEADER(SuggestionView, views::View)

   public:
    explicit SuggestionView(ContextualNudgeView* nudge_view)
        : nudge_view_(nudge_view) {
      SetPaintToLayer();
      layer()->SetFillsBoundsOpaquely(false);

      label_ = AddChildView(std::make_unique<views::Label>());
      label_->SetBackgroundColor(SK_ColorTRANSPARENT);
      label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
      label_->SetText(l10n_util::GetStringUTF16(
          base::i18n::IsRTL() ? IDS_ASH_BACK_GESTURE_CONTEXTUAL_NUDGE_RTL
                              : IDS_ASH_BACK_GESTURE_CONTEXTUAL_NUDGE));
      label_->SetLineHeight(kLabelLineHeight);
      label_->SetFontList(
          gfx::FontList().DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    }
    SuggestionView(const SuggestionView&) = delete;
    SuggestionView& operator=(const SuggestionView&) = delete;

    ~SuggestionView() override { StopObservingImplicitAnimations(); }

    void ScheduleBounceAnimation() {
      const bool is_rtl = base::i18n::IsRTL();
      gfx::Transform transform;
      const int x_offset = kCircleRadius - kCircleInsideScreenWidth;
      transform.Translate(is_rtl ? x_offset : -x_offset, 0);
      ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
      animation.AddObserver(this);
      animation.SetTransitionDuration(kSuggestionBounceAnimationDuration);
      animation.SetTweenType(gfx::Tween::EASE_IN_OUT_2);
      animation.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      layer()->SetTransform(transform);

      transform.Translate(is_rtl ? -x_offset : x_offset, 0);
      animation.SetTransitionDuration(kSuggestionBounceAnimationDuration);
      animation.SetTweenType(gfx::Tween::EASE_IN_OUT_2);
      animation.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      layer()->SetTransform(transform);
    }

    // Called when the in-progress animation is cancelled. The suggestion view
    // will fade out.
    void FadeOutForDismiss() {
      ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
      animation.SetTransitionDuration(kSuggestionDismissDuration);
      animation.SetTweenType(gfx::Tween::LINEAR);
      animation.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      animation.AddObserver(nudge_view_);
      layer()->SetOpacity(0.f);
    }

   private:
    // views::View:
    void Layout(PassKey) override {
      const gfx::Rect bounds = GetLocalBounds();
      gfx::Rect label_rect(bounds);
      label_rect.ClampToCenteredSize(
          label_->GetPreferredSize(views::SizeBounds(label_->width(), {})));
      label_rect.set_x(bounds.x() + 2 * kCircleRadius +
                       kPaddingBetweenCircleAndLabel + kLabelCornerRadius);
      label_->SetBoundsRect(label_rect);
    }

    // views::View:
    void OnPaint(gfx::Canvas* canvas) override {
      const auto* color_provider = GetColorProvider();
      // Draw the circle.
      cc::PaintFlags circle_flags;
      circle_flags.setAntiAlias(true);
      circle_flags.setStyle(cc::PaintFlags::kFill_Style);
      circle_flags.setColor(
          color_provider->GetColor(kColorAshShieldAndBaseOpaque));

      gfx::PointF center_point;
      if (base::i18n::IsRTL()) {
        const gfx::Point right_center = GetLocalBounds().right_center();
        center_point =
            gfx::PointF(right_center.x() - kCircleRadius, right_center.y());
      } else {
        const gfx::Point left_center = GetLocalBounds().left_center();
        center_point =
            gfx::PointF(left_center.x() + kCircleRadius, left_center.y());
      }
      canvas->DrawCircle(center_point, kCircleRadius, circle_flags);

      // Draw highlight border circles for the affordance.
      DrawCircleHighlightBorder(this, canvas, center_point, kCircleRadius);

      // Draw the black round rectangle around the text.
      cc::PaintFlags round_rect_flags;
      round_rect_flags.setStyle(cc::PaintFlags::kFill_Style);
      round_rect_flags.setAntiAlias(true);
      round_rect_flags.setColor(
          color_provider->GetColor(kColorAshShieldAndBaseOpaque));
      gfx::Rect label_bounds(label_->GetMirroredBounds());
      label_bounds.Inset(
          gfx::Insets::VH(-kLabelTopBottomInset, -kLabelCornerRadius));
      canvas->DrawRoundRect(label_bounds, kLabelCornerRadius, round_rect_flags);

      // Draw highlight border for the black round rectangle around the text.
      DrawRoundRectHighlightBorder(this, canvas, label_bounds,
                                   kLabelCornerRadius);
    }

    // ui::ImplicitAnimationObserver:
    void OnImplicitAnimationsCompleted() override {
      // Do not do the following animation if the bouncing animation is aborted.
      if (WasAnimationAbortedForProperty(ui::LayerAnimationElement::TRANSFORM))
        return;

      if (current_animation_times_ < (kSuggestionAnimationRepeatTimes - 1)) {
        current_animation_times_++;
        ScheduleBounceAnimation();
      } else {
        nudge_view_->ScheduleStartPositionToOffScreenAnimation();
      }
    }

    raw_ptr<views::Label> label_ = nullptr;
    int current_animation_times_ = 0;
    raw_ptr<ContextualNudgeView> nudge_view_ = nullptr;  // Not owned.
  };

  // Showing contextual nudge from off screen to its start position.
  void ScheduleOffScreenToStartPositionAnimation() {
    animation_stage_ = AnimationStage::kSlidingIn;
    gfx::Transform transform;
    transform.Translate(base::i18n::IsRTL() ? -kBackgroundWidth + kCircleRadius
                                            : kBackgroundWidth - kCircleRadius,
                        0);
    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.AddObserver(this);
    animation.SetTransitionDuration(kNudgeShowAnimationDuration);
    animation.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    layer()->SetTransform(transform);
  }

  // Hiding the contextual nudge from its current position to off screen.
  void ScheduleStartPositionToOffScreenAnimation() {
    animation_stage_ = AnimationStage::kSlidingOut;
    gfx::Transform transform;
    transform.Translate(base::i18n::IsRTL() ? kBackgroundWidth - kCircleRadius
                                            : -kBackgroundWidth + kCircleRadius,
                        0);
    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.SetTransitionDuration(kNudgeHideAnimationDuration);
    animation.SetTweenType(gfx::Tween::EASE_OUT_2);
    animation.AddObserver(this);
    layer()->SetTransform(transform);
  }

  void SetNudgeCountsAsShown() {
    count_as_shown_ = true;
    // Log nudge metrics right after it's shown.
    contextual_tooltip::HandleNudgeShown(
        Shell::Get()->session_controller()->GetActivePrefService(),
        contextual_tooltip::TooltipType::kBackGesture);
  }

  // views::View:
  void Layout(PassKey) override {
    suggestion_view_->SetBoundsRect(GetLocalBounds());
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (animation_stage_ == AnimationStage::kFadingOut ||
        (animation_stage_ == AnimationStage::kSlidingOut &&
         !WasAnimationAbortedForProperty(
             ui::LayerAnimationElement::TRANSFORM))) {
      std::move(callback_).Run(/*animation_completed=*/animation_stage_ ==
                               AnimationStage::kSlidingOut);
      return;
    }

    if (animation_stage_ == AnimationStage::kSlidingIn &&
        !WasAnimationAbortedForProperty(ui::LayerAnimationElement::TRANSFORM)) {
      // Only after the back nudge finishes sliding in animation, it counts as
      // a successful shown.
      SetNudgeCountsAsShown();
      animation_stage_ = AnimationStage::kBouncing;
      suggestion_view_->ScheduleBounceAnimation();
    }
  }

  // Created by ContextualNudgeView. Owned by views hierarchy.
  raw_ptr<SuggestionView> suggestion_view_ = nullptr;

  // Timer to start show the sliding in animation.
  base::OneShotTimer show_timer_;

  // Current animation stage;
  AnimationStage animation_stage_ = AnimationStage::kWaiting;

  // The nudge should be counted as shown if the nudge has finished its sliding-
  // in animation no matter whether its following animations get cancelled or
  // not.
  bool count_as_shown_ = false;

  // Callback function to be called after animation is cancelled or completed.
  // Count the nudge as shown successfully if |count_as_shown_| is true.
  base::OnceCallback<void(bool)> callback_;
};

BEGIN_METADATA(BackGestureContextualNudge, ContextualNudgeView)
END_METADATA

BEGIN_METADATA(BackGestureContextualNudge::ContextualNudgeView, SuggestionView)
END_METADATA

BackGestureContextualNudge::BackGestureContextualNudge(
    base::OnceCallback<void(bool)> callback) {
  widget_ = CreateWidget();
  nudge_view_ = widget_->SetContentsView(
      std::make_unique<ContextualNudgeView>(std::move(callback)));
  widget_->Show();
}

BackGestureContextualNudge::~BackGestureContextualNudge() = default;

void BackGestureContextualNudge::CancelAnimationOrFadeOutToHide() {
  nudge_view_->CancelAnimationOrFadeOutToHide();
}

bool BackGestureContextualNudge::ShouldNudgeCountAsShown() const {
  return nudge_view_->count_as_shown();
}

void BackGestureContextualNudge::SetNudgeShownForTesting() {
  nudge_view_->SetNudgeShownForTesting();
}

}  // namespace ash
