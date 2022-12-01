// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/multitask_menu_nudge_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_toast_style.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "base/check_is_test.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/wm/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

constexpr base::TimeDelta kNudgeDismissTimeout = base::Seconds(6);

// The name of an integer pref that counts the number of times we have shown the
// multitask menu educational nudge.
constexpr char kShownCountPrefName[] =
    "ash.wm_nudge.multitask_menu_nudge_count";

// The name of a time pref that stores the time we last showed the multitask
// menu education nudge.
constexpr char kLastShownPrefName[] =
    "ash.wm_nudge.multitask_menu_nudge_last_shown";

// The nudge will not be shown if it already been shown 3 times, or if 24 hours
// have not yet passed since it was last shown.
constexpr int kNudgeMaxShownCount = 3;
constexpr base::TimeDelta kNudgeTimeBetweenShown = base::Hours(24);

constexpr base::TimeDelta kFadeDuration = base::Milliseconds(50);

constexpr int kNudgeDistanceFromAnchor = 8;

// The max pulse size will be three times the size of the maximize/restore
// button.
constexpr float kPulseSizeMultiplier = 3.0f;
constexpr base::TimeDelta kPulseDuration = base::Seconds(2);
constexpr int kPulses = 3;

bool g_suppress_nudge_for_testing = false;

// Clock that can be overridden for testing.
base::Clock* g_clock_override = nullptr;

base::Time GetTime() {
  return g_clock_override ? g_clock_override->Now() : base::Time::Now();
}

namespace {

std::unique_ptr<views::Widget> CreateWidget(aura::Window* parent) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = "MultitaskNudgeWidget";
  params.accept_events = false;
  params.parent = parent;

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<SystemToastStyle>(
      base::DoNothing(),
      l10n_util::GetStringUTF16(IDS_MULTITASK_MENU_NUDGE_TEXT),
      /*dismiss_text=*/u"", /*is_managed=*/false));
  return widget;
}

}  // namespace

MultitaskMenuNudgeController::MultitaskMenuNudgeController() = default;

MultitaskMenuNudgeController::~MultitaskMenuNudgeController() {
  DismissNudgeInternal();
}

// static
void MultitaskMenuNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kShownCountPrefName, 0);
  registry->RegisterTimePref(kLastShownPrefName, base::Time());
}

void MultitaskMenuNudgeController::MaybeShowNudge(aura::Window* window) {
  if (!chromeos::wm::features::IsFloatWindowEnabled())
    return;

  if (g_suppress_nudge_for_testing)
    return;

  // Nudge is already being shown, possibly on a different window.
  if (nudge_widget_)
    return;

  // TODO(sammiequon): Tablet mode nudge will be different as there is no title
  // bar with resize button.
  if (Shell::Get()->tablet_mode_controller()->InTabletMode())
    return;

  // Only regular users can see the nudge.
  auto* session_controller = Shell::Get()->session_controller();
  const absl::optional<user_manager::UserType> user_type =
      session_controller->GetUserType();
  if (!user_type || *user_type != user_manager::USER_TYPE_REGULAR)
    return;

  // TODO(sammiequon): Once the multitask menu has been opened once, we don't
  // need to show the nudge anymore.
  // Nudge has already been shown three times. No need to educate anymore.
  auto* pref_service = session_controller->GetActivePrefService();
  DCHECK(pref_service);
  const int shown_count = pref_service->GetInteger(kShownCountPrefName);
  if (shown_count >= kNudgeMaxShownCount)
    return;

  // Nudge has been shown within the last 24 hours already.
  const base::Time time = GetTime();
  if (time - pref_service->GetTime(kLastShownPrefName) <
      kNudgeTimeBetweenShown) {
    return;
  }

  // Some tests create windows without a backing widget (`CreateTestWindow()`),
  // and some widgets may not have a header, test or custom header.
  auto* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget) {
    CHECK_IS_TEST();
    return;
  }

  auto* frame_header = chromeos::FrameHeader::Get(widget);
  if (!frame_header)
    return;

  // The anchor is the button on the header that serves as the maximize or
  // restore button (depending on the window state).
  auto* anchor_view = frame_header->caption_button_container()->size_button();
  DCHECK(anchor_view);

  // If either the window or anchor is not visible, do not show the nudge.
  if (!window->IsVisible() || !anchor_view->IsDrawn())
    return;

  window_ = window;
  window_observation_.Observe(window_);

  anchor_view_ = anchor_view;

  nudge_widget_ = CreateWidget(window_->parent());
  nudge_widget_->Show();

  // Create the layer which pulses on the maximize/restore button.
  pulse_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  pulse_layer_->SetColor(SK_ColorGRAY);
  window_->parent()->layer()->Add(pulse_layer_.get());

  UpdateWidgetAndPulse();
  DCHECK(nudge_widget_);

  // Fade the educational nudge in.
  ui::Layer* layer = nudge_widget_->GetLayer();
  layer->SetOpacity(0.0f);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kFadeDuration)
      .SetOpacity(layer, 1.0f, gfx::Tween::LINEAR);

  PerformPulseAnimation(/*pulse_count=*/0);

  // Update the preferences.
  pref_service->SetInteger(kShownCountPrefName, shown_count + 1);
  pref_service->SetTime(kLastShownPrefName, time);

  nudge_dismiss_timer_.Start(
      FROM_HERE, kNudgeDismissTimeout, this,
      &MultitaskMenuNudgeController::OnDismissTimerEnded);
}

void MultitaskMenuNudgeController::OnWindowParentChanged(aura::Window* window,
                                                         aura::Window* parent) {
  if (!parent)
    return;

  DCHECK_EQ(window_, window);
  UpdateWidgetAndPulse();
}

void MultitaskMenuNudgeController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (window_ == window)
    UpdateWidgetAndPulse();
}

void MultitaskMenuNudgeController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(window_, window);
  UpdateWidgetAndPulse();
}

void MultitaskMenuNudgeController::OnWindowTargetTransformChanging(
    aura::Window* window,
    const gfx::Transform& new_transform) {
  DCHECK_EQ(window_, window);
  DismissNudgeInternal();
}

void MultitaskMenuNudgeController::OnWindowStackingChanged(
    aura::Window* window) {
  DCHECK_EQ(window_, window);

  // Stacking may change during the construction of the widget, at which
  // `nudge_widget_` would still be null.
  if (!nudge_widget_)
    return;

  // Ensure the `nudge_widget_` is always above `window_`. We dont worry about
  // the pulse layer since it is not a window, and won't get stacked on top of
  // during window activation for example.
  window_->parent()->StackChildAbove(nudge_widget_->GetNativeWindow(), window);
}

void MultitaskMenuNudgeController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);
  DismissNudgeInternal();
}

void MultitaskMenuNudgeController::SetSuppressNudgeForTesting(bool val) {
  g_suppress_nudge_for_testing = val;
}

// static
void MultitaskMenuNudgeController::SetOverrideClockForTesting(
    base::Clock* test_clock) {
  g_clock_override = test_clock;
}

void MultitaskMenuNudgeController::OnDismissTimerEnded() {
  if (!nudge_widget_)
    return;

  ui::Layer* layer = nudge_widget_->GetLayer();
  layer->SetOpacity(1.0f);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(
          base::BindOnce(&MultitaskMenuNudgeController::DismissNudgeInternal,
                         base::Unretained(this)))
      .Once()
      .SetDuration(kFadeDuration)
      .SetOpacity(layer, 0.0f, gfx::Tween::LINEAR);
}

void MultitaskMenuNudgeController::DismissNudgeInternal() {
  nudge_dismiss_timer_.Stop();
  window_observation_.Reset();
  window_ = nullptr;
  anchor_view_ = nullptr;
  pulse_layer_.reset();
  if (nudge_widget_) {
    nudge_widget_->GetLayer()->GetAnimator()->AbortAllAnimations();
    nudge_widget_->CloseNow();
  }
}

void MultitaskMenuNudgeController::UpdateWidgetAndPulse() {
  DCHECK(nudge_widget_);
  DCHECK(pulse_layer_);
  DCHECK(window_);
  DCHECK(anchor_view_);

  // Dismiss the nudge if either of these are not visible, otherwise it will be
  // floating.
  if (!window_->IsVisible() || !anchor_view_->IsDrawn()) {
    DismissNudgeInternal();
    return;
  }

  const gfx::Rect anchor_bounds_in_screen = anchor_view_->GetBoundsInScreen();

  // Reparent the nudge and pulse if necessary.
  aura::Window* new_root =
      window_util::GetRootWindowMatching(anchor_bounds_in_screen);
  aura::Window* nudge_window = nudge_widget_->GetNativeWindow();

  if (new_root != nudge_window->GetRootWindow()) {
    const int parent_id = nudge_window->parent()->GetId();
    aura::Window* new_parent = new_root->GetChildById(parent_id);
    new_parent->AddChild(nudge_window);
    new_parent->layer()->Add(pulse_layer_.get());
  }

  // The nudge is placed right below the anchor.
  // TODO(crbug.com/1329233): Determine what to do if the nudge is offscreen.
  const gfx::Size size = nudge_widget_->GetContentsView()->GetPreferredSize();
  const gfx::Rect bounds_in_screen(
      anchor_bounds_in_screen.CenterPoint().x() - size.width() / 2,
      anchor_bounds_in_screen.bottom() + kNudgeDistanceFromAnchor, size.width(),
      size.height());
  nudge_widget_->SetBounds(bounds_in_screen);

  // The circular pulse should be a square that matches the smaller dimension of
  // `anchor_view_`. We use rounded corners to make it look like a circle.
  gfx::Rect pulse_layer_bounds = anchor_bounds_in_screen;
  wm::ConvertRectFromScreen(nudge_window->parent(), &pulse_layer_bounds);
  const int length =
      std::min(pulse_layer_bounds.width(), pulse_layer_bounds.height());
  pulse_layer_bounds.ClampToCenteredSize(gfx::Size(length, length));
  pulse_layer_->SetBounds(pulse_layer_bounds);
  pulse_layer_->SetRoundedCornerRadius(gfx::RoundedCornersF(length / 2.f));
}

void MultitaskMenuNudgeController::PerformPulseAnimation(int pulse_count) {
  if (pulse_count >= kPulses)
    return;

  DCHECK(pulse_layer_);

  // The pulse animation scales up and fades out on top of the maximize/restore
  // button until the nudge disappears.
  const gfx::Point pivot(
      gfx::Rect(pulse_layer_->GetTargetBounds().size()).CenterPoint());
  const gfx::Transform transform =
      gfx::GetScaleTransform(pivot, kPulseSizeMultiplier);

  pulse_layer_->SetOpacity(1.0f);
  pulse_layer_->SetTransform(gfx::Transform());

  // Note that `views::AnimationBuilder::Repeatedly` works here as well, but
  // causes tests to hang.
  views::AnimationBuilder builder;
  builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(
          base::BindOnce(&MultitaskMenuNudgeController::PerformPulseAnimation,
                         base::Unretained(this), pulse_count + 1))
      .Once()
      .SetDuration(kPulseDuration)
      .SetOpacity(pulse_layer_.get(), 0.0f, gfx::Tween::ACCEL_0_80_DECEL_80)
      .SetTransform(pulse_layer_.get(), transform,
                    gfx::Tween::ACCEL_0_40_DECEL_100);
}

}  // namespace ash
