// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_admin_session_controller.h"

#include <iomanip>
#include <ostream>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/remote_support_host_ash.h"
#include "remoting/host/chromeos/remoting_service.h"
#include "remoting/host/chromeos/session_id.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "remoting/protocol/errors.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

using AccessCodeCallback = StartCrdSessionJobDelegate::AccessCodeCallback;
using ErrorCallback = StartCrdSessionJobDelegate::ErrorCallback;
using SessionEndCallback = StartCrdSessionJobDelegate::SessionEndCallback;
using SessionParameters = StartCrdSessionJobDelegate::SessionParameters;
using remoting::features::kEnableCrdAdminRemoteAccessV2;

namespace {

// Default implementation of the `RemotingService`, which will contact the real
// remoting service.
class DefaultRemotingService
    : public CrdAdminSessionController::RemotingServiceProxy {
 public:
  DefaultRemotingService() = default;
  DefaultRemotingService(const DefaultRemotingService&) = delete;
  DefaultRemotingService& operator=(const DefaultRemotingService&) = delete;
  ~DefaultRemotingService() override = default;

  // `CrdAdminSessionController::RemotingService` implementation:
  void StartSession(remoting::mojom::SupportSessionParamsPtr params,
                    const remoting::ChromeOsEnterpriseParams& enterprise_params,
                    StartSessionCallback callback) override {
    return GetSupportHost().StartSession(*params.get(), enterprise_params,
                                         std::move(callback));
  }

  void GetReconnectableSessionId(SessionIdCallback callback) override {
    return GetService().GetReconnectableEnterpriseSessionId(
        std::move(callback));
  }

  void ReconnectToSession(remoting::SessionId session_id,
                          StartSessionCallback callback) override {
    return GetSupportHost().ReconnectToSession(session_id, std::move(callback));
  }

 private:
  remoting::RemotingService& GetService() {
    return remoting::RemotingService::Get();
  }

  remoting::RemoteSupportHostAsh& GetSupportHost() {
    return remoting::RemotingService::Get().GetSupportHost();
  }
};

std::ostream& operator<<(std::ostream& os,
                         const SessionParameters& parameters) {
  return os << "{ "
            << "user_name " << std::quoted(parameters.user_name)
            << ", admin_email "
            << std::quoted(parameters.admin_email.value_or("<null>"))
            << ", terminate_upon_input " << parameters.terminate_upon_input
            << ", show_confirmation_dialog "
            << parameters.show_confirmation_dialog
            << ", curtain_local_user_session "
            << parameters.curtain_local_user_session
            << ", show_troubleshooting_tools "
            << parameters.show_troubleshooting_tools
            << ", allow_troubleshooting_tools "
            << parameters.allow_troubleshooting_tools
            << ", allow_reconnections " << parameters.allow_reconnections
            << "}";
}

class SupportHostObserver : public remoting::mojom::SupportHostObserver {
 public:
  SupportHostObserver(AccessCodeCallback success_callback,
                      ErrorCallback error_callback,
                      SessionEndCallback session_finished_callback)
      : success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)),
        session_finished_callback_(std::move(session_finished_callback)) {}

  SupportHostObserver(const SupportHostObserver&) = delete;
  SupportHostObserver& operator=(const SupportHostObserver&) = delete;
  ~SupportHostObserver() override = default;

  void Bind(
      mojo::PendingReceiver<remoting::mojom::SupportHostObserver> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // `remoting::mojom::SupportHostObserver` implementation:
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

  void ReportError(ResultCode error_code, const std::string& error_message) {
    if (error_callback_) {
      std::move(error_callback_).Run(error_code, error_message);
      success_callback_.Reset();
    }
  }

 private:
  void ReportSuccess(const std::string& access_code) {
    if (success_callback_) {
      std::move(success_callback_).Run(access_code);
      error_callback_.Reset();
    }
  }

  void ReportSessionTermination(base::TimeDelta session_duration) {
    if (session_finished_callback_) {
      std::move(session_finished_callback_).Run(session_duration);
    }
  }

  AccessCodeCallback success_callback_;
  ErrorCallback error_callback_;
  SessionEndCallback session_finished_callback_;
  absl::optional<base::Time> session_connected_time_;
  mojo::Receiver<remoting::mojom::SupportHostObserver> receiver_{this};
};

remoting::mojom::SupportSessionParamsPtr GetSessionParameters(
    const SessionParameters& parameters) {
  auto result = remoting::mojom::SupportSessionParams::New();
  result->user_name = parameters.user_name;
  result->authorized_helper = parameters.admin_email;
  // Note the oauth token must be prefixed with 'oauth2:', or it will be
  // rejected by the CRD host.
  result->oauth_access_token = "oauth2:" + parameters.oauth_token;

  return result;
}

remoting::ChromeOsEnterpriseParams GetEnterpriseParameters(
    const SessionParameters& parameters) {
  return remoting::ChromeOsEnterpriseParams{
      .suppress_user_dialogs = !parameters.show_confirmation_dialog,
      .suppress_notifications = !parameters.show_confirmation_dialog,
      .terminate_upon_input = parameters.terminate_upon_input,
      .curtain_local_user_session = parameters.curtain_local_user_session,
      .show_troubleshooting_tools = parameters.show_troubleshooting_tools,
      .allow_troubleshooting_tools = parameters.allow_troubleshooting_tools,
      .allow_reconnections = parameters.allow_reconnections,
      .allow_file_transfer = parameters.allow_file_transfer,
  };
}
}  // namespace

class CrdAdminSessionController::CrdHostSession {
 public:
  explicit CrdHostSession(RemotingServiceProxy& remoting_service)
      : CrdHostSession(remoting_service,
                       base::DoNothing(),
                       base::DoNothing(),
                       base::DoNothing()) {}
  CrdHostSession(RemotingServiceProxy& remoting_service,
                 AccessCodeCallback success_callback,
                 ErrorCallback error_callback,
                 SessionEndCallback session_finished_callback)
      : remoting_service_(remoting_service),
        observer_(std::move(success_callback),
                  std::move(error_callback),
                  std::move(session_finished_callback)) {}
  CrdHostSession(const CrdHostSession&) = delete;
  CrdHostSession& operator=(const CrdHostSession&) = delete;
  ~CrdHostSession() = default;

  void Start(const SessionParameters& parameters) {
    CRD_DVLOG(3) << "Starting CRD session with parameters " << parameters;

    remoting_service_->StartSession(
        GetSessionParameters(parameters), GetEnterpriseParameters(parameters),
        base::BindOnce(&CrdHostSession::OnStartSupportSessionResponse,
                       weak_factory_.GetWeakPtr()));
  }

  void TryToReconnect(base::OnceClosure done_callback) {
    CRD_DVLOG(3) << "Checking for reconnectable session";
    remoting_service_->GetReconnectableSessionId(
        base::BindOnce(&CrdHostSession::ReconnectToSession,
                       weak_factory_.GetWeakPtr())
            .Then(std::move(done_callback)));
  }

 private:
  void ReconnectToSession(absl::optional<remoting::SessionId> id) {
    if (id.has_value()) {
      CRD_LOG(INFO) << "Attempting to resume reconnectable session";

      remoting_service_->ReconnectToSession(
          id.value(),
          base::BindOnce(&CrdHostSession::OnStartSupportSessionResponse,
                         weak_factory_.GetWeakPtr()));
    } else {
      CRD_DVLOG(3) << "No reconnectable CRD session found.";
    }
  }

  void OnStartSupportSessionResponse(
      remoting::mojom::StartSupportSessionResponsePtr response) {
    if (response->is_support_session_error()) {
      // Since `observer_` owns all the callbacks we must ask it to invoke the
      // error callback.
      observer_.ReportError(ResultCode::FAILURE_CRD_HOST_ERROR, "");
      return;
    }

    observer_.Bind(std::move(response->get_observer()));
  }

  raw_ref<RemotingServiceProxy> remoting_service_;
  SupportHostObserver observer_;

  base::WeakPtrFactory<CrdHostSession> weak_factory_{this};
};

CrdAdminSessionController::CrdAdminSessionController()
    : CrdAdminSessionController(std::make_unique<DefaultRemotingService>()) {}

CrdAdminSessionController::CrdAdminSessionController(
    std::unique_ptr<RemotingServiceProxy> remoting_service)
    : remoting_service_(std::move(remoting_service)) {}

CrdAdminSessionController::~CrdAdminSessionController() = default;

void CrdAdminSessionController::Init(base::OnceClosure done_callback) {
  if (base::FeatureList::IsEnabled(kEnableCrdAdminRemoteAccessV2)) {
    TryToReconnect(std::move(done_callback));
  } else {
    std::move(done_callback).Run();
  }
}

StartCrdSessionJobDelegate& CrdAdminSessionController::GetDelegate() {
  return *this;
}

bool CrdAdminSessionController::HasActiveSession() const {
  return active_session_ != nullptr;
}

void CrdAdminSessionController::TerminateSession(base::OnceClosure callback) {
  CRD_DVLOG(3) << "Terminating CRD session";
  active_session_ = nullptr;
  std::move(callback).Run();
}

void CrdAdminSessionController::TryToReconnect(
    base::OnceClosure done_callback) {
  CHECK(!active_session_);

  active_session_ = std::make_unique<CrdHostSession>(*remoting_service_);
  active_session_->TryToReconnect(std::move(done_callback));
}

void CrdAdminSessionController::StartCrdHostAndGetCode(
    const SessionParameters& parameters,
    AccessCodeCallback success_callback,
    ErrorCallback error_callback,
    SessionEndCallback session_finished_callback) {
  CHECK(!active_session_);
  active_session_ = std::make_unique<CrdHostSession>(
      *remoting_service_, std::move(success_callback),
      std::move(error_callback), std::move(session_finished_callback));

  active_session_->Start(parameters);
}

// static
void CrdAdminSessionController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kRemoteAdminWasPresent, false);
}

}  // namespace policy
