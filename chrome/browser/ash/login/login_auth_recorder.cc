// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/login_auth_recorder.h"

#include <optional>
#include <string>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

using AuthMethod = LoginAuthRecorder::AuthMethod;
using AuthMethodSwitchType = LoginAuthRecorder::AuthMethodSwitchType;

std::optional<AuthMethodSwitchType> SwitchFromPasswordTo(AuthMethod current) {
  CHECK_NE(AuthMethod::kPassword, current, base::NotFatalUntil::M160);
  switch (current) {
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kPasswordToPin;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kPasswordToSmartlock;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kPasswordToFingerprint;
    case AuthMethod::kChallengeResponse:
      return AuthMethodSwitchType::kPasswordToChallengeResponse;
    case AuthMethod::kPassword:
    case AuthMethod::kNothing:
      NOTREACHED();
  }
}

std::optional<AuthMethodSwitchType> SwitchFromPinTo(AuthMethod current) {
  CHECK_NE(AuthMethod::kPin, current, base::NotFatalUntil::M160);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kPinToPassword;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kPinToSmartlock;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kPinToFingerprint;
    case AuthMethod::kPin:
    case AuthMethod::kChallengeResponse:
    case AuthMethod::kNothing:
      NOTREACHED();
  }
}

std::optional<AuthMethodSwitchType> SwitchFromSmartlockTo(AuthMethod current) {
  CHECK_NE(AuthMethod::kSmartlock, current, base::NotFatalUntil::M160);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kSmartlockToPassword;
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kSmartlockToPin;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kSmartlockToFingerprint;
    case AuthMethod::kSmartlock:
    case AuthMethod::kChallengeResponse:
    case AuthMethod::kNothing:
      NOTREACHED();
  }
}

std::optional<AuthMethodSwitchType> SwitchFromFingerprintTo(
    AuthMethod current) {
  CHECK_NE(AuthMethod::kFingerprint, current, base::NotFatalUntil::M160);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kFingerprintToPassword;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kFingerprintToSmartlock;
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kFingerprintToPin;
    case AuthMethod::kFingerprint:
    case AuthMethod::kChallengeResponse:
    case AuthMethod::kNothing:
      NOTREACHED();
  }
}

std::optional<AuthMethodSwitchType> SwitchFromNothingTo(AuthMethod current) {
  CHECK_NE(AuthMethod::kNothing, current, base::NotFatalUntil::M160);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kNothingToPassword;
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kNothingToPin;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kNothingToSmartlock;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kNothingToFingerprint;
    case AuthMethod::kChallengeResponse:
      return AuthMethodSwitchType::kNothingToChallengeResponse;
    case AuthMethod::kNothing:
      NOTREACHED();
  }
}

std::optional<AuthMethodSwitchType> FindSwitchType(AuthMethod previous,
                                                   AuthMethod current) {
  CHECK_NE(previous, current, base::NotFatalUntil::M160);
  switch (previous) {
    case AuthMethod::kPassword:
      return SwitchFromPasswordTo(current);
    case AuthMethod::kPin:
      return SwitchFromPinTo(current);
    case AuthMethod::kSmartlock:
      return SwitchFromSmartlockTo(current);
    case AuthMethod::kFingerprint:
      return SwitchFromFingerprintTo(current);
    case AuthMethod::kNothing:
      return SwitchFromNothingTo(current);
    case AuthMethod::kChallengeResponse:
      NOTREACHED();
  }
}

}  // namespace

LoginAuthRecorder::LoginAuthRecorder() {
  session_manager::SessionManager::Get()->AddObserver(this);
}

LoginAuthRecorder::~LoginAuthRecorder() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void LoginAuthRecorder::RecordAuthMethod(AuthMethod method) {
  CHECK_NE(method, AuthMethod::kNothing, base::NotFatalUntil::M160);

  bool is_locked;
  switch (session_manager::SessionManager::Get()->session_state()) {
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOGIN_SECONDARY:
      is_locked = false;
      break;
    case session_manager::SessionState::LOCKED:
      is_locked = true;
      break;
    default:
      return;
  }
  const std::string prefix =
      is_locked ? "Ash.Login.Lock.AuthMethod." : "Ash.Login.Login.AuthMethod.";

  // Record usage of the authentication method in login/lock screen.
  base::UmaHistogramEnumeration(
      base::StrCat(
          {prefix, "Used.",
           (display::Screen::Get()->InTabletMode() ? "TabletMode"
                                                   : "ClamShellMode")}),
      method);

  if (last_auth_method_ != method) {
    // Record switching between unlock methods.
    const std::optional<AuthMethodSwitchType> switch_type =
        FindSwitchType(last_auth_method_, method);
    if (switch_type) {
      base::UmaHistogramEnumeration(base::StrCat({prefix, "Switched"}),
                                    *switch_type);
    }

    last_auth_method_ = method;
  }
}

void LoginAuthRecorder::OnSessionStateChanged() {
  // Reset local state.
  last_auth_method_ = AuthMethod::kNothing;
}

}  // namespace ash
