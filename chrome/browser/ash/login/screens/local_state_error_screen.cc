// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/local_state_error_screen.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/local_state_error_screen_handler.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

namespace ash {
namespace {

constexpr char kUserActionRestartAndPowerwash[] = "restart-and-powerwash";

}  // namespace

LocalStateErrorScreen::LocalStateErrorScreen(
    base::WeakPtr<LocalStateErrorScreenView> view)
    : BaseScreen(LocalStateErrorScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)) {
  DCHECK(view_);
}

LocalStateErrorScreen::~LocalStateErrorScreen() = default;

void LocalStateErrorScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void LocalStateErrorScreen::HideImpl() {}

void LocalStateErrorScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionRestartAndPowerwash) {
    SessionManagerClient::Get()->StartDeviceWipe(base::DoNothing());
    return;
  }
  BaseScreen::OnUserAction(args);
}

}  // namespace ash
