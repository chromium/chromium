// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/local_authentication_test_api.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/auth/views/active_session_auth_view.h"
#include "ash/auth/views/auth_common.h"
#include "ash/auth/views/auth_view_utils.h"
#include "ash/login/ui/local_authentication_request_view.h"
#include "ash/login/ui/local_authentication_request_widget.h"
#include "ash/public/cpp/login/local_authentication_test_api.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"

namespace ash {

std::unique_ptr<LocalAuthenticationTestApi> GetLocalAuthenticationTestApi(
    LocalAuthenticationRequestController* controller) {
  if (controller->IsPinSupported()) {
    return std::unique_ptr<LocalAuthenticationTestApi>(
        new LocalAuthenticationWithPinTestApi(
            static_cast<LocalAuthenticationWithPinControllerImpl*>(
                controller)));
  }
  return std::unique_ptr<LocalAuthenticationTestApi>(
      new LocalAuthenticationRequestControllerImplTestApi());
}

LocalAuthenticationRequestControllerImplTestApi::
    LocalAuthenticationRequestControllerImplTestApi() = default;
LocalAuthenticationRequestControllerImplTestApi::
    ~LocalAuthenticationRequestControllerImplTestApi() = default;

void LocalAuthenticationRequestControllerImplTestApi::SubmitPassword(
    const std::string& password) {
  bool dialog_exists =
      LocalAuthenticationRequestWidget::TestApi::SubmitPassword(password);
  CHECK(dialog_exists);
}

void LocalAuthenticationRequestControllerImplTestApi::SubmitPin(
    const std::string& pin) {
  NOTIMPLEMENTED();
}

void LocalAuthenticationRequestControllerImplTestApi::Close() {
  bool dialog_exists =
      LocalAuthenticationRequestWidget::TestApi::CancelDialog();
  CHECK(dialog_exists);
}

LocalAuthenticationWithPinTestApi::LocalAuthenticationWithPinTestApi(
    LocalAuthenticationWithPinControllerImpl* controller)
    : controller_(controller) {}

LocalAuthenticationWithPinTestApi::~LocalAuthenticationWithPinTestApi() =
    default;

AuthFactorSet LocalAuthenticationWithPinTestApi::GetAvailableFactors() const {
  return controller_->available_factors_;
}

void LocalAuthenticationWithPinTestApi::SubmitPassword(
    const std::string& password) {
  controller_->OnPasswordSubmit(base::UTF8ToUTF16(password));
}

void LocalAuthenticationWithPinTestApi::SubmitPin(const std::string& pin) {
  controller_->OnPinSubmit(base::UTF8ToUTF16(pin));
}

void LocalAuthenticationWithPinTestApi::Close() {
  using PinState =
      LocalAuthenticationWithPinControllerImpl::LocalAuthenticationWithPinState;

  switch (controller_->state_) {
    case PinState::kWaitForInit:
      return;
    case PinState::kInitialized:
      controller_->StartClose();
      return;
    case PinState::kPasswordAuthStarted:
    case PinState::kPinAuthStarted:
      controller_->SetState(PinState::kCloseRequested);
      return;
    case PinState::kPasswordAuthSucceeded:
    case PinState::kPinAuthSucceeded:
    case PinState::kCloseRequested:
      return;
  }
  NOTREACHED();
}

void LocalAuthenticationWithPinTestApi::SetPinStatus(
    std::unique_ptr<cryptohome::PinStatus> pin_status) {
  controller_->contents_view_->SetPinStatus(std::move(pin_status));
}

const std::u16string& LocalAuthenticationWithPinTestApi::GetPinStatusMessage()
    const {
  return controller_->contents_view_->GetPinStatusMessage();
}

raw_ptr<ActiveSessionAuthView>
LocalAuthenticationWithPinTestApi::GetContentsView() {
  return controller_->contents_view_;
}

}  // namespace ash
