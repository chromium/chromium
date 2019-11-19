// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/pin_dialog_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"

namespace chromeos {

// Define timeout for issued sign_request_id.
constexpr base::TimeDelta kSignRequestIdTimeout =
    base::TimeDelta::FromMinutes(10);

PinDialogManager::PinDialogManager() = default;

PinDialogManager::~PinDialogManager() = default;

void PinDialogManager::AddSignRequestId(
    const std::string& extension_id,
    int sign_request_id,
    const base::Optional<AccountId>& authenticating_user_account_id) {
  ExtensionNameRequestIdPair key(extension_id, sign_request_id);
  sign_requests_.insert(
      std::make_pair(key, SignRequestState(/*begin_time=*/base::Time::Now(),
                                           authenticating_user_account_id)));
}

void PinDialogManager::AbortSignRequest(const std::string& extension_id,
                                        int sign_request_id) {
  if (active_dialog_state_ &&
      active_dialog_state_->extension_id == extension_id &&
      active_dialog_state_->sign_request_id == sign_request_id) {
    CloseActiveDialog();
  }

  ExtensionNameRequestIdPair key(extension_id, sign_request_id);
  sign_requests_.erase(key);
}

PinDialogManager::RequestPinResult PinDialogManager::RequestPin(
    const std::string& extension_id,
    const std::string& extension_name,
    int sign_request_id,
    SecurityTokenPinCodeType code_type,
    SecurityTokenPinErrorLabel error_label,
    int attempts_left,
    RequestPinCallback callback) {
  DCHECK_GE(attempts_left, -1);
  const bool accept_input = (attempts_left != 0);

  // Check the validity of sign_request_id.
  const SignRequestState* const sign_request_state =
      FindSignRequestState(extension_id, sign_request_id);
  if (!sign_request_state)
    return RequestPinResult::kInvalidId;

  // Start from sanity checks, as the extension might have issued this call
  // incorrectly.
  if (active_dialog_state_) {
    // The active dialog exists already, so we need to make sure it belongs to
    // the same extension and the user submitted some input.
    if (extension_id != active_dialog_state_->extension_id)
      return RequestPinResult::kOtherFlowInProgress;
    if (active_dialog_state_->request_pin_callback ||
        active_dialog_state_->stop_pin_request_callback) {
      // Extension requests a PIN without having received any input from its
      // previous request. Reject the new request.
      return RequestPinResult::kDialogDisplayedAlready;
    }
  } else {
    // Check that the sign request hasn't timed out yet.
    const base::Time current_time = base::Time::Now();
    if (current_time - sign_request_state->begin_time > kSignRequestIdTimeout)
      return RequestPinResult::kInvalidId;

    // A new dialog will be opened, so initialize the related internal state.
    active_dialog_state_.emplace(GetHostForNewDialog(), extension_id,
                                 extension_name, sign_request_id, code_type);
  }

  active_dialog_state_->request_pin_callback = std::move(callback);
  active_dialog_state_->host->ShowSecurityTokenPinDialog(
      extension_name, code_type, accept_input, error_label, attempts_left,
      sign_request_state->authenticating_user_account_id,
      base::BindOnce(&PinDialogManager::OnPinEntered,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&PinDialogManager::OnPinDialogClosed,
                     weak_factory_.GetWeakPtr()));

  return RequestPinResult::kSuccess;
}

PinDialogManager::StopPinRequestResult
PinDialogManager::StopPinRequestWithError(
    const std::string& extension_id,
    SecurityTokenPinErrorLabel error_label,
    StopPinRequestCallback callback) {
  DCHECK_NE(error_label, SecurityTokenPinErrorLabel::kNone);

  // Perform sanity checks, as the extension might have issued this call
  // incorrectly.
  if (!active_dialog_state_ ||
      active_dialog_state_->extension_id != extension_id) {
    return StopPinRequestResult::kNoActiveDialog;
  }
  if (active_dialog_state_->request_pin_callback ||
      active_dialog_state_->stop_pin_request_callback) {
    return StopPinRequestResult::kNoUserInput;
  }

  const SignRequestState* const sign_request_state =
      FindSignRequestState(extension_id, active_dialog_state_->sign_request_id);
  if (!sign_request_state)
    return StopPinRequestResult::kNoActiveDialog;

  active_dialog_state_->stop_pin_request_callback = std::move(callback);
  active_dialog_state_->host->ShowSecurityTokenPinDialog(
      active_dialog_state_->extension_name, active_dialog_state_->code_type,
      /*enable_user_input=*/false, error_label,
      /*attempts_left=*/-1, sign_request_state->authenticating_user_account_id,
      base::BindOnce(&PinDialogManager::OnPinEntered,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&PinDialogManager::OnPinDialogClosed,
                     weak_factory_.GetWeakPtr()));

  return StopPinRequestResult::kSuccess;
}

bool PinDialogManager::LastPinDialogClosed(
    const std::string& extension_id) const {
  auto iter = last_response_closed_.find(extension_id);
  return iter != last_response_closed_.end() && iter->second;
}

bool PinDialogManager::CloseDialog(const std::string& extension_id) {
  // Perform sanity checks, as the extension might have issued this call
  // incorrectly.
  if (!active_dialog_state_ ||
      extension_id != active_dialog_state_->extension_id) {
    LOG(ERROR) << "StopPinRequest called by unexpected extension: "
               << extension_id;
    return false;
  }

  CloseActiveDialog();
  return true;
}

void PinDialogManager::ExtensionUnloaded(const std::string& extension_id) {
  if (active_dialog_state_ &&
      active_dialog_state_->extension_id == extension_id) {
    CloseActiveDialog();
  }

  last_response_closed_[extension_id] = false;

  for (auto it = sign_requests_.cbegin(); it != sign_requests_.cend();) {
    if (it->first.first == extension_id)
      sign_requests_.erase(it++);
    else
      ++it;
  }
}

void PinDialogManager::AddPinDialogHost(
    SecurityTokenPinDialogHost* pin_dialog_host) {
  DCHECK(!base::Contains(added_dialog_hosts_, pin_dialog_host));
  added_dialog_hosts_.push_back(pin_dialog_host);
}

void PinDialogManager::RemovePinDialogHost(
    SecurityTokenPinDialogHost* pin_dialog_host) {
  if (active_dialog_state_ && active_dialog_state_->host == pin_dialog_host)
    CloseActiveDialog();
  DCHECK(base::Contains(added_dialog_hosts_, pin_dialog_host));
  base::Erase(added_dialog_hosts_, pin_dialog_host);
}

PinDialogManager::SignRequestState::SignRequestState(
    base::Time begin_time,
    const base::Optional<AccountId>& authenticating_user_account_id)
    : begin_time(begin_time),
      authenticating_user_account_id(authenticating_user_account_id) {}

PinDialogManager::SignRequestState::SignRequestState(const SignRequestState&) =
    default;
PinDialogManager::SignRequestState& PinDialogManager::SignRequestState::
operator=(const SignRequestState&) = default;

PinDialogManager::SignRequestState::~SignRequestState() = default;

PinDialogManager::ActiveDialogState::ActiveDialogState(
    SecurityTokenPinDialogHost* host,
    const std::string& extension_id,
    const std::string& extension_name,
    int sign_request_id,
    SecurityTokenPinCodeType code_type)
    : host(host),
      extension_id(extension_id),
      extension_name(extension_name),
      sign_request_id(sign_request_id),
      code_type(code_type) {}

PinDialogManager::ActiveDialogState::~ActiveDialogState() = default;

PinDialogManager::SignRequestState* PinDialogManager::FindSignRequestState(
    const std::string& extension_id,
    int sign_request_id) {
  const ExtensionNameRequestIdPair key(extension_id, sign_request_id);
  const auto sign_request_iter = sign_requests_.find(key);
  if (sign_request_iter == sign_requests_.end())
    return nullptr;
  return &sign_request_iter->second;
}

void PinDialogManager::OnPinEntered(const std::string& user_input) {
  DCHECK(!active_dialog_state_->stop_pin_request_callback);
  last_response_closed_[active_dialog_state_->extension_id] = false;
  if (active_dialog_state_->request_pin_callback)
    std::move(active_dialog_state_->request_pin_callback).Run(user_input);
}

void PinDialogManager::OnPinDialogClosed() {
  DCHECK(!active_dialog_state_->request_pin_callback ||
         !active_dialog_state_->stop_pin_request_callback);

  last_response_closed_[active_dialog_state_->extension_id] = true;
  if (active_dialog_state_->request_pin_callback) {
    std::move(active_dialog_state_->request_pin_callback)
        .Run(/*user_input=*/std::string());
  }
  if (active_dialog_state_->stop_pin_request_callback)
    std::move(active_dialog_state_->stop_pin_request_callback).Run();
  active_dialog_state_.reset();
}

SecurityTokenPinDialogHost* PinDialogManager::GetHostForNewDialog() {
  if (added_dialog_hosts_.empty())
    return &default_dialog_host_;
  return added_dialog_hosts_.back();
}

void PinDialogManager::CloseActiveDialog() {
  if (!active_dialog_state_)
    return;

  // Ignore any further callbacks from the host. Instead of relying on the host
  // to call the closing callback, run OnPinDialogClosed() below explicitly.
  weak_factory_.InvalidateWeakPtrs();

  active_dialog_state_->host->CloseSecurityTokenPinDialog();
  OnPinDialogClosed();
  DCHECK(!active_dialog_state_);
}

}  // namespace chromeos
