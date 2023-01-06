// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/logged_in_user_enabled_provider.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"

namespace ash {
namespace quick_pair {

bool ShouldBeEnabledForLoginStatus(LoginStatus status) {
  switch (status) {
    case LoginStatus::NOT_LOGGED_IN:
    case LoginStatus::LOCKED:
    case LoginStatus::KIOSK_APP:
    case LoginStatus::PUBLIC:
      return false;
    case LoginStatus::USER:
    case LoginStatus::GUEST:
    case LoginStatus::CHILD:
    default:
      return true;
  }
}

LoggedInUserEnabledProvider::LoggedInUserEnabledProvider() {
  auto* session_controller = Shell::Get()->session_controller();
  observation_.Observe(session_controller);
  SetEnabledAndInvokeCallback(
      ShouldBeEnabledForLoginStatus(session_controller->login_status()));
}

LoggedInUserEnabledProvider::~LoggedInUserEnabledProvider() = default;

void LoggedInUserEnabledProvider::OnLoginStatusChanged(
    LoginStatus login_status) {
  SetEnabledAndInvokeCallback(ShouldBeEnabledForLoginStatus(login_status));
}

}  // namespace quick_pair
}  // namespace ash
