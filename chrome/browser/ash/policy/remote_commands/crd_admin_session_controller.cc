// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_admin_session_controller.h"

#include <iomanip>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/crd_session_observer.h"
#include "chrome/browser/ash/policy/remote_commands/crd_support_host_observer_proxy.h"
#include "chrome/browser/ash/policy/remote_commands/remote_activity_notification_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/remote_support_host_ash.h"
#include "remoting/host/chromeos/remoting_service.h"
#include "remoting/host/chromeos/session_id.h"
#include "remoting/host/mojom/remote_support.mojom.h"
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

// Will invoke the given `success_callback` if the host started successfully,
// or the `error_callback` if it failed to launch for any reason.
class HostStartObserver : public CrdSessionObserver {
 public:
  HostStartObserver(AccessCodeCallback success_callback,
                    ErrorCallback error_callback)
      : success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)) {}

  HostStartObserver(const HostStartObserver&) = delete;
  HostStartObserver& operator=(const HostStartObserver&) = delete;
  ~HostStartObserver() override = default;

  // `CrdSessionObserver` implementation:
  void OnHostStarted(const std::string& access_code) override {
    if (success_callback_) {
      std::move(success_callback_).Run(access_code);
      error_callback_.Reset();
    }
  }

  void OnHostStopped(ResultCode result, const std::string& message) override {
    if (error_callback_) {
      std::move(error_callback_).Run(result, message);
      success_callback_.Reset();
    }
  }

 private:
  AccessCodeCallback success_callback_;
  ErrorCallback error_callback_;
};

class SessionDurationObserver : public CrdSessionObserver {
 public:
  explicit SessionDurationObserver(SessionEndCallback callback)
      : callback_(std::move(callback)) {}
  SessionDurationObserver(const SessionDurationObserver&) = delete;
  SessionDurationObserver& operator=(const SessionDurationObserver&) = delete;
  ~SessionDurationObserver() override = default;

  // `CrdSessionObserver` implementation:
  void OnClientConnected() override {
    session_connected_time_ = base::Time::Now();
  }

  void OnClientDisconnected() override {
    if (session_connected_time_.has_value() && callback_) {
      std::move(callback_).Run(base::Time::Now() -
                               session_connected_time_.value());
    }
  }

 private:
  SessionEndCallback callback_;
  absl::optional<base::Time> session_connected_time_;
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

class CrdAdminSessionController::CrdHostSession : private CrdSessionObserver {
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
      : remoting_service_(remoting_service) {
    AddOwnedObserver(std::make_unique<HostStartObserver>(
        std::move(success_callback), std::move(error_callback)));
    AddOwnedObserver(std::make_unique<SessionDurationObserver>(
        std::move(session_finished_callback)));
    AddObserver(this);
  }
  CrdHostSession(const CrdHostSession&) = delete;
  CrdHostSession& operator=(const CrdHostSession&) = delete;
  ~CrdHostSession() override = default;

  void Start(const SessionParameters& parameters) {
    CRD_DVLOG(3) << "Starting CRD session with parameters " << parameters;
    session_parameters_ = parameters;

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

  void AddObserver(CrdSessionObserver* observer) {
    observer_proxy_.AddObserver(observer);
  }

  bool IsSessionCurtained() const {
    return session_parameters_.has_value() &&
           session_parameters_->curtain_local_user_session;
  }

  // We only have a valid, active CRD session (to which the remote admin
  // can connect/is connected) as long as the CRD host is bound.
  bool IsHostBound() const { return observer_proxy_.IsBound(); }

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

  void AddOwnedObserver(std::unique_ptr<CrdSessionObserver> observer) {
    observer_proxy_.AddObserver(observer.get());
    owned_session_observers_.push_back(std::move(observer));
  }

  void OnStartSupportSessionResponse(
      remoting::mojom::StartSupportSessionResponsePtr response) {
    if (response->is_support_session_error()) {
      // Since `observer_proxy_` owns all the callbacks we must ask it to invoke
      // the error callback.
      observer_proxy_.ReportHostStopped(ResultCode::FAILURE_CRD_HOST_ERROR, "");
      return;
    }

    observer_proxy_.Bind(std::move(response->get_observer()));
  }

  // `CrdSessionObserver` implementation:
  void OnHostStopped(ResultCode, const std::string&) override {
    // Signal the CRD host has stopped by unbinding our observer, which will
    // allow the remoting code to do a full cleanup.
    observer_proxy_.Unbind();
  }

  raw_ref<RemotingServiceProxy> remoting_service_;

  SupportHostObserverProxy observer_proxy_;
  std::vector<std::unique_ptr<CrdSessionObserver>> owned_session_observers_;
  absl::optional<SessionParameters> session_parameters_;

  base::WeakPtrFactory<CrdHostSession> weak_factory_{this};
};

CrdAdminSessionController::CrdAdminSessionController()
    : CrdAdminSessionController(std::make_unique<DefaultRemotingService>()) {}

CrdAdminSessionController::CrdAdminSessionController(
    std::unique_ptr<RemotingServiceProxy> remoting_service)
    : remoting_service_(std::move(remoting_service)) {}

CrdAdminSessionController::~CrdAdminSessionController() = default;

void CrdAdminSessionController::Init(PrefService* local_state,
                                     base::OnceClosure done_callback) {
  if (base::FeatureList::IsEnabled(kEnableCrdAdminRemoteAccessV2)) {
    TryToReconnect(std::move(done_callback));

    CHECK(!notification_controller_);
    notification_controller_ =
        std::make_unique<RemoteActivityNotificationController>(
            CHECK_DEREF(local_state),
            base::BindRepeating(
                &CrdAdminSessionController::IsCurrentSessionCurtained,
                base::Unretained(this)));
  } else {
    std::move(done_callback).Run();
  }
}

void CrdAdminSessionController::ClickNotificationButtonForTesting() {
  CHECK(notification_controller_);
  CHECK_IS_TEST();
  notification_controller_->ClickNotificationButtonForTesting();  // IN-TEST
}

StartCrdSessionJobDelegate& CrdAdminSessionController::GetDelegate() {
  return *this;
}

bool CrdAdminSessionController::HasActiveSession() const {
  return active_session_ != nullptr && active_session_->IsHostBound();
}

void CrdAdminSessionController::TerminateSession() {
  CRD_DVLOG(3) << "Terminating CRD session";
  active_session_ = nullptr;
}

void CrdAdminSessionController::TryToReconnect(
    base::OnceClosure done_callback) {
  CHECK(!HasActiveSession());

  active_session_ = std::make_unique<CrdHostSession>(*remoting_service_);
  active_session_->TryToReconnect(std::move(done_callback));
}

void CrdAdminSessionController::StartCrdHostAndGetCode(
    const SessionParameters& parameters,
    AccessCodeCallback success_callback,
    ErrorCallback error_callback,
    SessionEndCallback session_finished_callback) {
  CHECK(!HasActiveSession());
  active_session_ = std::make_unique<CrdHostSession>(
      *remoting_service_, std::move(success_callback),
      std::move(error_callback), std::move(session_finished_callback));

  if (base::FeatureList::IsEnabled(kEnableCrdAdminRemoteAccessV2)) {
    active_session_->AddObserver(notification_controller_.get());
  }

  active_session_->Start(parameters);
}

bool CrdAdminSessionController::IsCurrentSessionCurtained() const {
  return active_session_ && active_session_->IsSessionCurtained();
}

// static
void CrdAdminSessionController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kRemoteAdminWasPresent, false);
  registry->RegisterBooleanPref(
      prefs::kRemoteAccessHostAllowEnterpriseRemoteSupportConnections, true);
}

}  // namespace policy
