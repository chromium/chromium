// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/password_access_auth_timeout_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/password_manager/core/common/password_manager_constants.h"

namespace extensions {

PasswordAccessAuthTimeoutHandler::PasswordAccessAuthTimeoutHandler() = default;

PasswordAccessAuthTimeoutHandler::~PasswordAccessAuthTimeoutHandler() = default;

void PasswordAccessAuthTimeoutHandler::Init(TimeoutCallback timeout_call) {
  timeout_call_ = std::move(timeout_call);
}

void PasswordAccessAuthTimeoutHandler::RestartAuthTimer() {
  if (timeout_timer_.IsRunning()) {
    timeout_timer_.Reset();
  }
}

void PasswordAccessAuthTimeoutHandler::OnUserReauthenticationResult(
    bool authenticated) {
  if (authenticated) {
    CHECK(!timeout_call_.is_null());
    timeout_timer_.Start(
        FROM_HERE, password_manager::constants::kPasswordManagerAuthValidity,
        base::BindRepeating(timeout_call_));
  }
}

}  // namespace extensions
