// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_support_host_observer_proxy.h"

#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/crd_session_observer.h"
#include "remoting/protocol/errors.h"

using remoting::protocol::ErrorCode;
using remoting::protocol::ErrorCodeToString;

namespace policy {

SupportHostObserverProxy::SupportHostObserverProxy() = default;
SupportHostObserverProxy::~SupportHostObserverProxy() = default;

void SupportHostObserverProxy::AddObserver(CrdSessionObserver* observer) {
  observers_.AddObserver(observer);
}

void SupportHostObserverProxy::Bind(
    mojo::PendingReceiver<remoting::mojom::SupportHostObserver> receiver) {
  receiver_.Bind(std::move(receiver));
}

void SupportHostObserverProxy::Unbind() {
  receiver_.reset();
}

bool SupportHostObserverProxy::IsBound() const {
  return receiver_.is_bound();
}

// `remoting::mojom::SupportHostObserver` implementation:
void SupportHostObserverProxy::OnHostStateStarting() {
  CRD_DVLOG(3) << __func__;
}

void SupportHostObserverProxy::OnHostStateRequestedAccessCode() {
  CRD_DVLOG(3) << __func__;
}

void SupportHostObserverProxy::OnHostStateReceivedAccessCode(
    const std::string& access_code,
    base::TimeDelta lifetime) {
  CRD_DVLOG(3) << __func__;

  for (auto& observer : observers_) {
    observer.OnHostStarted(access_code);
  }
}

void SupportHostObserverProxy::OnHostStateConnecting() {
  CRD_DVLOG(3) << __func__;
}

void SupportHostObserverProxy::OnHostStateConnected(
    const std::string& remote_username) {
  CRD_DVLOG(3) << __func__;

  for (auto& observer : observers_) {
    observer.OnClientConnected();
  }
}

void SupportHostObserverProxy::OnHostStateDisconnected(
    const absl::optional<std::string>& disconnect_reason) {
  // We always want to log this event, as it could help customers debug why
  // their CRD connection is failing/disconnecting.
  LOG(WARNING) << "CRD client disconnected with reason: "
               << disconnect_reason.value_or("<none>");

  for (auto& observer : observers_) {
    observer.OnClientDisconnected();
  }

  ReportHostStopped(ResultCode::HOST_SESSION_DISCONNECTED,
                    "client disconnected");
}

void SupportHostObserverProxy::OnNatPolicyChanged(
    remoting::mojom::NatPolicyStatePtr nat_policy_state) {
  CRD_DVLOG(3) << __func__;
}

void SupportHostObserverProxy::OnHostStateError(int64_t error) {
  const ErrorCode error_code = static_cast<ErrorCode>(error);

  CRD_DVLOG(3) << __func__
               << " with error code: " << ErrorCodeToString(error_code) << "("
               << error_code << ")";

  const ResultCode result_code = ConvertErrorCodeToResultCode(error_code);
  ReportHostStopped(result_code, "host state error");
}

void SupportHostObserverProxy::OnPolicyError() {
  CRD_DVLOG(3) << __func__;

  ReportHostStopped(ResultCode::FAILURE_HOST_POLICY_ERROR, "policy error");
}

void SupportHostObserverProxy::OnInvalidDomainError() {
  CRD_DVLOG(3) << __func__;

  ReportHostStopped(ResultCode::FAILURE_HOST_INVALID_DOMAIN_ERROR,
                    "invalid domain error");
}

void SupportHostObserverProxy::ReportHostStopped(
    ResultCode error_code,
    const std::string& error_message) {
  for (auto& observer : observers_) {
    observer.OnHostStopped(error_code, error_message);
  }
}

}  // namespace policy
