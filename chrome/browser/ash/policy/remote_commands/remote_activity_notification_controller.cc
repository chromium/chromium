// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/remote_activity_notification_controller.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/policy/remote_commands/remote_activity_notification_view.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/compositor/layer.h"

namespace policy {

namespace {

views::Widget::InitParams GetWidgetInitParams(aura::Window* parent) {
  views::Widget::InitParams result;
  result.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  result.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  result.name = "AdminWasPresentNotificationWidget";
  result.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  result.parent = parent;
  result.z_order = ui::ZOrderLevel::kSecuritySurface;
  return result;
}

std::unique_ptr<views::Widget> CreateWidget(
    aura::Window* parent,
    std::unique_ptr<views::View> contents) {
  auto widget = std::make_unique<views::Widget>();
  widget->Init(GetWidgetInitParams(parent));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  widget->SetContentsView(std::move(contents));
  widget->SetSize(gfx::Size(400, 200));
  widget->GetNativeWindow()->SetId(
      ash::kShellWindowId_AdminWasPresentNotificationWindow);
  return widget;
}

}  // namespace

RemoteActivityNotificationController::RemoteActivityNotificationController(
    PrefService& local_state,
    base::RepeatingCallback<bool()> is_current_session_curtained)
    : local_state_(local_state),
      is_current_session_curtained_(is_current_session_curtained) {
  observation_.Observe(session_manager::SessionManager::Get());
}

RemoteActivityNotificationController::~RemoteActivityNotificationController() =
    default;

// We only can display UI elements (like the notification) after the login
// screen is properly initialized.
void RemoteActivityNotificationController::OnLoginOrLockScreenVisible() {
  Init();
}

void RemoteActivityNotificationController::OnCurtainSessionStarted() {
  local_state_->SetBoolean(prefs::kRemoteAdminWasPresent, true);
}

// TODO(b/299143143): Add id for the new UI view and check if the button click
// is tested.
void RemoteActivityNotificationController::ClickNotificationButtonForTesting() {
  CHECK_IS_TEST();
  OnNotificationCloseButtonClick();  // IN-TEST
}

void RemoteActivityNotificationController::OnNotificationCloseButtonClick() {
  if (widget_) {
    widget_->Hide();
    widget_.reset();
  }

  // To make sure remote admin cannot dismiss the notification permanently
  // themselves by starting a new session.
  if (is_current_session_curtained_.Run()) {
    return;
  }

  local_state_->SetBoolean(prefs::kRemoteAdminWasPresent, false);
}

void RemoteActivityNotificationController::Init() {
  if (local_state_->GetBoolean(prefs::kRemoteAdminWasPresent)) {
    ShowNotification();
  }
}

void RemoteActivityNotificationController::ShowNotification() {
  CHECK(ash::Shell::HasInstance());
  CHECK(ash::Shell::GetPrimaryRootWindow()->GetRootWindow());

  widget_ = CreateWidget(
      ash::Shell::GetPrimaryRootWindow()->GetChildById(
          ash::kShellWindowId_LockScreenContainersContainer),
      std::make_unique<RemoteActivityNotificationView>(base::BindRepeating(
          &RemoteActivityNotificationController::OnNotificationCloseButtonClick,
          base::Unretained(this))));
  widget_->Show();
}

}  // namespace policy
