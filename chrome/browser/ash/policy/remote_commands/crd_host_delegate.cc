// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_host_delegate.h"

#include <iomanip>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"
#include "remoting/host/chromeos/remote_support_host_ash.h"
#include "remoting/host/chromeos/remoting_service.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "remoting/protocol/errors.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

using AccessCodeCallback = DeviceCommandStartCrdSessionJob::AccessCodeCallback;
using ErrorCallback = DeviceCommandStartCrdSessionJob::ErrorCallback;
using SessionEndCallback = DeviceCommandStartCrdSessionJob::SessionEndCallback;

namespace {

// Default implementation of the `RemotingService`, which will contact the real
// remoting service.
class DefaultRemotingService : public CrdHostDelegate::RemotingServiceProxy {
 public:
  DefaultRemotingService() = default;
  DefaultRemotingService(const DefaultRemotingService&) = delete;
  DefaultRemotingService& operator=(const DefaultRemotingService&) = delete;
  ~DefaultRemotingService() override = default;

  // `CrdHostDelegate::RemotingService` implementation:
  void StartSession(remoting::mojom::SupportSessionParamsPtr params,
                    const remoting::ChromeOsEnterpriseParams& enterprise_params,
                    StartSessionCallback callback) override {
    return remoting::RemotingService::Get().GetSupportHost().StartSession(
        std::move(params), enterprise_params, std::move(callback));
  }
};

}  // namespace

class CrdHostDelegate::CrdHostSession
    : public remoting::mojom::SupportHostObserver {
 public:
  CrdHostSession(const SessionParameters& parameters,
                 AccessCodeCallback success_callback,
                 ErrorCallback error_callback,
                 SessionEndCallback session_finished_callback)
      : parameters_(parameters),
        success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)),
        session_finished_callback_(std::move(session_finished_callback)) {}
  CrdHostSession(const CrdHostSession&) = delete;
  CrdHostSession& operator=(const CrdHostSession&) = delete;
  ~CrdHostSession() override = default;

  void Start(CrdHostDelegate::RemotingServiceProxy& remoting_service) {
    CRD_DVLOG(3) << "Starting CRD session with parameters { "
                 << "user_name " << std::quoted(parameters_.user_name)
                 << ", admin_email "
                 << std::quoted(parameters_.admin_email.value_or("<null>"))
                 << ", terminate_upon_input "
                 << parameters_.terminate_upon_input
                 << ", show_confirmation_dialog "
                 << parameters_.show_confirmation_dialog
                 << ", curtain_local_user_session "
                 << parameters_.curtain_local_user_session
                 << ", allow_troubleshooting_tools "
                 << parameters_.allow_troubleshooting_tools << "}";

    remoting_service.StartSession(
        GetSessionParameters(), GetEnterpriseParameters(),
        base::BindOnce(&CrdHostSession::OnStartSupportSessionResponse,
                       weak_factory_.GetWeakPtr()));
  }

  // remoting::mojom::SupportHostObserver implementation:
  void OnHostStateStarting() override { CRD_DVLOG(3) << __FUNCTION__; }
  void OnHostStateRequestedAccessCode() override {
    CRD_DVLOG(3) << __FUNCTION__;
  }
  void OnHostStateReceivedAccessCode(const std::string& access_code,
                                     base::TimeDelta lifetime) override {
    CRD_DVLOG(3) << __FUNCTION__;

    ReportSuccess(access_code);
  }
  void OnHostStateConnecting() override { CRD_DVLOG(3) << __FUNCTION__; }
  void OnHostStateConnected(const std::string& remote_username) override {
    CRD_DVLOG(3) << __FUNCTION__;
    session_connected_time_ = base::Time::Now();
  }
  void OnHostStateDisconnected(
      const absl::optional<std::string>& disconnect_reason) override {
    // We always want to log this event, as it could help customers debug why
    // their CRD connection is failing/disconnecting.
    LOG(WARNING) << "CRD session disconnected with reason: "
                 << disconnect_reason.value_or("<none>");
    if (session_connected_time_.has_value()) {
      ReportSessionTermination(base::Time::Now() -
                               session_connected_time_.value());
    }
    ReportError(ResultCode::FAILURE_CRD_HOST_ERROR, "host disconnected");
  }
  void OnNatPolicyChanged(
      remoting::mojom::NatPolicyStatePtr nat_policy_state) override {
    CRD_DVLOG(3) << __FUNCTION__;
  }
  void OnHostStateError(int64_t error_code) override {
    CRD_DVLOG(3) << __FUNCTION__ << " with error code: " << error_code;

    if (error_code == remoting::protocol::ErrorCode::DISALLOWED_BY_POLICY) {
      ReportError(ResultCode::FAILURE_DISABLED_BY_POLICY,
                  "enterprise remote support disabled");
      return;
    }

    ReportError(ResultCode::FAILURE_CRD_HOST_ERROR, "host state error");
  }
  void OnPolicyError() override {
    CRD_DVLOG(3) << __FUNCTION__;

    ReportError(ResultCode::FAILURE_CRD_HOST_ERROR, "policy error");
  }
  void OnInvalidDomainError() override {
    CRD_DVLOG(3) << __FUNCTION__;

    ReportError(ResultCode::FAILURE_CRD_HOST_ERROR, "invalid domain error");
  }

 private:
  void OnStartSupportSessionResponse(
      remoting::mojom::StartSupportSessionResponsePtr response) {
    if (response->is_support_session_error()) {
      ReportError(ResultCode::FAILURE_CRD_HOST_ERROR, "");
      return;
    }

    observer_.Bind(std::move(response->get_observer()));
  }

  remoting::mojom::SupportSessionParamsPtr GetSessionParameters() const {
    auto result = remoting::mojom::SupportSessionParams::New();
    result->user_name = parameters_.user_name;
    result->authorized_helper = parameters_.admin_email;
    // Note the oauth token must be prefixed with 'oauth2:', or it will be
    // rejected by the CRD host.
    result->oauth_access_token = "oauth2:" + parameters_.oauth_token;

    return result;
  }

  remoting::ChromeOsEnterpriseParams GetEnterpriseParameters() const {
    return remoting::ChromeOsEnterpriseParams{
        .suppress_user_dialogs = !parameters_.show_confirmation_dialog,
        .suppress_notifications = !parameters_.show_confirmation_dialog,
        .terminate_upon_input = parameters_.terminate_upon_input,
        .curtain_local_user_session = parameters_.curtain_local_user_session,
        .allow_troubleshooting_tools = parameters_.allow_troubleshooting_tools};
  }

  void ReportSuccess(const std::string& access_code) {
    if (success_callback_) {
      std::move(success_callback_).Run(access_code);

      success_callback_.Reset();
      error_callback_.Reset();
    }
  }

  void ReportError(ResultCode error_code, const std::string& error_message) {
    if (error_callback_) {
      std::move(error_callback_).Run(error_code, error_message);

      success_callback_.Reset();
      error_callback_.Reset();
    }
  }

  void ReportSessionTermination(base::TimeDelta session_duration) {
    if (session_finished_callback_) {
      std::move(session_finished_callback_).Run(session_duration);

      session_finished_callback_.Reset();
    }
  }

  SessionParameters parameters_;
  AccessCodeCallback success_callback_;
  ErrorCallback error_callback_;
  SessionEndCallback session_finished_callback_;
  absl::optional<base::Time> session_connected_time_;

  mojo::Receiver<remoting::mojom::SupportHostObserver> observer_{this};
  base::WeakPtrFactory<CrdHostSession> weak_factory_{this};
};

CrdHostDelegate::CrdHostDelegate()
    : CrdHostDelegate(std::make_unique<DefaultRemotingService>()) {}

CrdHostDelegate::CrdHostDelegate(
    std::unique_ptr<RemotingServiceProxy> remoting_service)
    : remoting_service_(std::move(remoting_service)) {}

CrdHostDelegate::~CrdHostDelegate() = default;

bool CrdHostDelegate::HasActiveSession() const {
  return active_session_ != nullptr;
}

void CrdHostDelegate::TerminateSession(base::OnceClosure callback) {
  CRD_DVLOG(3) << "Terminating CRD session";
  active_session_ = nullptr;
  std::move(callback).Run();
}

void CrdHostDelegate::StartCrdHostAndGetCode(
    const SessionParameters& parameters,
    AccessCodeCallback success_callback,
    ErrorCallback error_callback,
    SessionEndCallback session_finished_callback) {
  DCHECK(!active_session_);
  active_session_ = std::make_unique<CrdHostSession>(
      parameters, std::move(success_callback), std::move(error_callback),
      std::move(session_finished_callback));

  active_session_->Start(*remoting_service_);
}

}  // namespace policy
