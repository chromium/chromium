// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/remote_activity_notification_screen.h"

#include "chrome/browser/ui/webui/ash/login/remote_activity_notification_screen_handler.h"

namespace ash {

namespace {

constexpr const char kUserActionContinueButtonClicked[] =
    "continue-using-device";

}  // namespace

RemoteActivityNotificationScreen::RemoteActivityNotificationScreen(
    base::WeakPtr<RemoteActivityNotificationView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(RemoteActivityNotificationView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

RemoteActivityNotificationScreen::~RemoteActivityNotificationScreen() = default;

RemoteActivityNotificationScreen::ScreenExitCallback
RemoteActivityNotificationScreen::exit_callback() {
  return exit_callback_;
}

void RemoteActivityNotificationScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  view_->Show();
}

void RemoteActivityNotificationScreen::HideImpl() {}

void RemoteActivityNotificationScreen::OnUserAction(
    const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionContinueButtonClicked) {
    exit_callback_.Run();
    return;
  }

  BaseScreen::OnUserAction(args);
}

}  // namespace ash
