// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/fullscreen_notification_bubble.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/fullscreen_control/subtle_notification_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr base::TimeDelta kAutoHideDelay = base::Seconds(4);
constexpr int kNotificationViewTopDp = 45;

}  // namespace

FullscreenNotificationBubble::FullscreenNotificationBubble()
    : timer_(std::make_unique<base::OneShotTimer>()) {
  // Create the notification view.
  auto subtle_notification_view = std::make_unique<SubtleNotificationView>();
  view_ = subtle_notification_view.get();

  // Set the text displayed in the bubble, including the keyboard key.
  view_->UpdateContent(l10n_util::GetStringFUTF16(
      IDS_FULLSCREEN_PRESS_TO_EXIT_FULLSCREEN,
      l10n_util::GetStringUTF16(IDS_APP_FULLSCREEN_KEY)));

  // The view used to parent the bubble widget
  const gfx::NativeView parent_view =
      Shell::GetPrimaryRootWindow()->GetChildById(
          kShellWindowId_SettingBubbleContainer);

  // Create the widget containing the SubtleNotificationView.
  widget_ = SubtleNotificationView::CreatePopupWidget(
      parent_view, std::move(subtle_notification_view));

  gfx::Rect rect = GetBubbleBounds();
  widget_->SetBounds(rect);
  widget_->SetZOrderLevel(ui::ZOrderLevel::kSecuritySurface);
}

FullscreenNotificationBubble::~FullscreenNotificationBubble() = default;

void FullscreenNotificationBubble::ShowForWindowState(
    WindowState* window_state) {
  // Early out if a WindowState is already tracked. The bubble should already be
  // visible in this case.
  if (window_state_observation_.IsObserving()) {
    // `window_state` arg should match the tracked `window_state_`.
    DCHECK(window_state_observation_.IsObservingSource(window_state));
    DCHECK(window_observation_.IsObservingSource(window_state->window()));
    return;
  }
  window_state_observation_.Observe(window_state);

  // Observe the window to properly handle window destruction.
  DCHECK(!window_observation_.IsObserving());
  window_observation_.Observe(window_state->window());

  Show();
}

void FullscreenNotificationBubble::Show() {
  if (widget_->IsVisible())
    return;

  widget_->Show();

  timer_->Start(FROM_HERE, kAutoHideDelay,
                base::BindOnce(&FullscreenNotificationBubble::Hide,
                               weak_ptr_factory_.GetWeakPtr()));
}

void FullscreenNotificationBubble::Hide() {
  window_observation_.Reset();
  window_state_observation_.Reset();

  if (!widget_->IsVisible())
    return;

  widget_->Hide();
}

void FullscreenNotificationBubble::OnWindowDestroying(aura::Window* window) {
  Hide();
}

void FullscreenNotificationBubble::OnPreWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  bool is_exiting_fullscreen =
      old_type == chromeos::WindowStateType::kFullscreen &&
      !window_state->IsFullscreen();
  if (is_exiting_fullscreen)
    Hide();
}

gfx::Rect FullscreenNotificationBubble::GetBubbleBounds() {
  gfx::Size size(view_->GetPreferredSize());
  gfx::Rect widget_bounds = Shell::GetPrimaryRootWindow()->GetBoundsInScreen();
  int x = widget_bounds.x() + (widget_bounds.width() - size.width()) / 2;

  // |desired_top| is the top of the bubble area including the shadow.
  const int desired_top = kNotificationViewTopDp - view_->GetInsets().top();
  const int y = widget_bounds.y() + desired_top;

  return gfx::Rect(gfx::Point(x, y), size);
}

}  // namespace ash
