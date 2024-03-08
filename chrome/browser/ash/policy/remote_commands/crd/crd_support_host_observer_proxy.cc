// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/crd_support_host_observer_proxy.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_logging.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_session_observer.h"
#include "remoting/protocol/errors.h"

using remoting::protocol::ErrorCode;
using remoting::protocol::ErrorCodeToString;

namespace policy {

SupportHostObserverProxy::SupportHostObserverProxy() = default;
SupportHostObserverProxy::~SupportHostObserverProxy() = default;

void SupportHostObserverProxy::AddObserver(CrdSessionObserver* observer) {
  observers_.AddObserver(observer);
}

void SupportHostObserverProxy::AddOwnedObserver(
    std::unique_ptr<CrdSessionObserver> observer) {
  AddObserver(observer.get());
  owned_session_observers_.push_back(std::move(observer));
}

void SupportHostObserverProxy::Bind(
    mojo::PendingReceiver<remoting::mojom::SupportHostObserver> receiver) {
  receiver_.Bind(std::move(receiver));

  // Inform our observers that the session has started
  for (auto& observer : observers_) {
    observer.OnHostStarted();
  }

  // Ensure we can inform our observers if the mojom connection drops.
  receiver_.set_disconnect_handler(
      base::BindOnce(&SupportHostObserverProxy::OnMojomConnectionDropped,
                     base::Unretained(this)));
}

// `remoting::mojom::SupportHostObserver` implementation:
void SupportHostObserverProxy::OnHostStateStarting() {
  CRD_VLOG(3) << __func__;
}

void SupportHostObserverProxy::OnHostStateRequestedAccessCode() {
  CRD_VLOG(3) << __func__;
}

void SupportHostObserverProxy::OnHostStateReceivedAccessCode(
    const std::string& access_code,
    base::TimeDelta lifetime) {
  CRD_VLOG(3) << __func__;

  for (auto& observer : observers_) {
    observer.OnAccessCodeReceived(access_code);
  }
}

void SupportHostObserverProxy::OnHostStateConnecting() {
  CRD_VLOG(3) << __func__;

  for (auto& observer : observers_) {
    observer.OnClientConnecting();
  }
}

void SupportHostObserverProxy::OnHostStateConnected(
    const std::string& remote_username) {
  CRD_VLOG(3) << __func__;

  for (auto& observer : observers_) {
    observer.OnClientConnected();
  }
}

void SupportHostObserverProxy::OnHostStateDisconnected(
    const std::optional<std::string>& disconnect_reason) {
  // We always want to log this event, as it could help customers debug why
  // their CRD connection is failing/disconnecting.
  LOG(WARNING) << "CRD client disconnected with reason: "
               << disconnect_reason.value_or("<none>");

  for (auto& observer : observers_) {
    observer.OnClientDisconnected();
  }

  ReportHostStopped(ExtendedStartCrdSessionResultCode::kHostSessionDisconnected,
                    "client disconnected");
}

void SupportHostObserverProxy::OnNatPolicyChanged(
    remoting::mojom::NatPolicyStatePtr nat_policy_state) {
  CRD_VLOG(3) << __func__;
}

void SupportHostObserverProxy::OnHostStateError(int64_t error) {
  const ErrorCode error_code = static_cast<ErrorCode>(error);

  CRD_VLOG(3) << __func__
              << " with error code: " << ErrorCodeToString(error_code) << "("
              << error << ")";

  ReportHostStopped(ToExtendedStartCrdSessionResultCode(error_code),
                    "host state error");
}

void SupportHostObserverProxy::OnPolicyError() {
  CRD_VLOG(3) << __func__;

  ReportHostStopped(ExtendedStartCrdSessionResultCode::kFailureHostPolicyError,
                    "policy error");
}

void SupportHostObserverProxy::OnInvalidDomainError() {
  CRD_VLOG(3) << __func__;

  ReportHostStopped(
      ExtendedStartCrdSessionResultCode::kFailureHostInvalidDomainError,
      "invalid domain error");
}

void SupportHostObserverProxy::OnMojomConnectionDropped() {
  CRD_VLOG(3) << "Mojom connection with CRD host dropped";

  ReportHostStopped(ExtendedStartCrdSessionResultCode::kFailureCrdHostError,
                    "mojom connection dropped");
}

void SupportHostObserverProxy::ReportHostStopped(
    ExtendedStartCrdSessionResultCode result,
    std::string_view error_message) {
  for (auto& observer : observers_) {
    observer.OnHostStopped(result, std::string{error_message});
  }
}

}  // namespace policy
