// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/multitask_menu_nudge_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/wm/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_caption_button.h"

namespace ash {

constexpr int kNudgeCornerRadius = 16;
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

// Clock that can be overridden for testing.
base::Clock* g_clock_override = nullptr;

base::Time GetTime() {
  return g_clock_override ? g_clock_override->Now() : base::Time::Now();
}

namespace {

std::unique_ptr<views::Widget> CreateWidget(aura::Window* root_window) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = "MultitaskNudgeWidget";
  params.accept_events = false;
  params.parent = root_window->GetChildById(kShellWindowId_OverlayContainer);

  auto widget = std::make_unique<views::Widget>(std::move(params));

  // The contents view is a view that contains an icon and a label.
  // TODO(crbug.com/1329233): The values, colors and text are all placeholders
  // until the spec is received.
  auto contents_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBackground(views::CreateRoundedRectBackground(SK_ColorGRAY,
                                                            kNudgeCornerRadius))
          .SetInsideBorderInsets(gfx::Insets(5))
          .SetBetweenChildSpacing(10)
          .AddChildren(
              views::Builder<views::ImageView>()
                  .SetPreferredSize(gfx::Size(30, 30))
                  .SetImage(gfx::CreateVectorIcon(
                      kPersistentDesksBarFeedbackIcon, 30, SK_ColorWHITE)),
              views::Builder<views::Label>()
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetText(
                      u"Keep hovering on maximize for more layout options"))
          .Build();
  widget->SetContentsView(std::move(contents_view));
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

  // Nudge is already being shown, possibly on a different window.
  if (nudge_widget_)
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

  window_ = window;
  window_observation_.Observe(window_);

  // The anchor is the button on the header that serves as the maximize or
  // restore button (depending on the window state).
  auto* frame_header = chromeos::FrameHeader::Get(
      views::Widget::GetWidgetForNativeWindow(window_));
  DCHECK(frame_header);
  anchor_view_ = frame_header->caption_button_container()->size_button();
  DCHECK(anchor_view_);

  nudge_widget_ = CreateWidget(window_->GetRootWindow());
  nudge_widget_->Show();
  UpdateWidgetBounds();

  // Fade the educational nudge in.
  ui::Layer* layer = nudge_widget_->GetLayer();
  layer->SetOpacity(0.0f);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kFadeDuration)
      .SetOpacity(layer, 1.0f, gfx::Tween::LINEAR);

  // Update the preferences.
  pref_service->SetInteger(kShownCountPrefName, shown_count + 1);
  pref_service->SetTime(kLastShownPrefName, time);

  nudge_dismiss_timer_.Start(
      FROM_HERE, kNudgeDismissTimeout, this,
      &MultitaskMenuNudgeController::OnDismissTimerEnded);
}

void MultitaskMenuNudgeController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);
  DismissNudgeInternal();
}

void MultitaskMenuNudgeController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(window_, window);
  UpdateWidgetBounds();
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
  if (nudge_widget_) {
    nudge_widget_->GetLayer()->GetAnimator()->AbortAllAnimations();
    nudge_widget_->CloseNow();
  }
}

void MultitaskMenuNudgeController::UpdateWidgetBounds() {
  DCHECK(nudge_widget_);
  DCHECK(window_);
  DCHECK(anchor_view_);

  // The nudge is placed right below the anchor, and shifted to fit in the
  // display if it is offscreen.
  const gfx::Rect anchor_bounds_in_screen = anchor_view_->GetBoundsInScreen();
  const gfx::Size size = nudge_widget_->GetContentsView()->GetPreferredSize();
  const gfx::Rect bounds_in_screen(
      anchor_bounds_in_screen.CenterPoint().x() - size.width() / 2,
      anchor_bounds_in_screen.bottom(), size.width(), size.height());
  nudge_widget_->SetBoundsConstrained(bounds_in_screen);
}

}  // namespace ash
