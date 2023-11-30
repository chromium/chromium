// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_admin_session_controller.h"

#include <iomanip>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/ash/policy/remote_commands/crd_oauth_token_fetcher.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/crd_session_observer.h"
#include "chrome/browser/ash/policy/remote_commands/crd_support_host_observer_proxy.h"
#include "chrome/browser/ash/policy/remote_commands/remote_activity_notification_controller.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/remote_support_host_ash.h"
#include "remoting/host/chromeos/remoting_service.h"
#include "remoting/host/chromeos/session_id.h"
#include "remoting/host/mojom/remote_support.mojom.h"

namespace policy {

using AccessCodeCallback = StartCrdSessionJobDelegate::AccessCodeCallback;
using ErrorCallback = StartCrdSessionJobDelegate::ErrorCallback;
using SessionEndCallback = StartCrdSessionJobDelegate::SessionEndCallback;
using SessionParameters = StartCrdSessionJobDelegate::SessionParameters;
using remoting::features::kEnableCrdAdminRemoteAccessV2;

namespace {

// Time after which an access code is guaranteed to have expired.
constexpr base::TimeDelta kAccessCodeMaxTTL = base::Minutes(15);

// Parameters used to start the actual `CrdHostSession` when the launcher
// finishes and we can finally start the session.
struct SessionStartParameters {
  bool curtained = false;

  mojo::PendingReceiver<remoting::mojom::SupportHostObserver> host_observer;
};

using SessionLaunchResult =
    base::expected<SessionStartParameters, ExtendedStartCrdSessionResultCode>;

template <typename T>
void DeleteSoon(std::unique_ptr<T> value) {
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                             std::move(value));
}

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
                          const std::string& oauth_access_token,
                          StartSessionCallback callback) override {
    return GetSupportHost().ReconnectToSession(session_id, oauth_access_token,
                                               std::move(callback));
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

// Will invoke the given `success_callback` if the host managed to generate an
// access code, or the `error_callback` if it failed to launch for any reason.
class AccessCodeObserver : public CrdSessionObserver {
 public:
  AccessCodeObserver(AccessCodeCallback success_callback,
                     ErrorCallback error_callback)
      : success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)) {}

  AccessCodeObserver(const AccessCodeObserver&) = delete;
  AccessCodeObserver& operator=(const AccessCodeObserver&) = delete;
  ~AccessCodeObserver() override = default;

  // `CrdSessionObserver` implementation:
  void OnAccessCodeReceived(const std::string& access_code) override {
    if (success_callback_) {
      std::move(success_callback_).Run(access_code);
      error_callback_.Reset();
    }
  }

  void OnHostStopped(ExtendedStartCrdSessionResultCode result,
                     const std::string& message) override {
    if (error_callback_) {
      std::move(error_callback_).Run(result, message);
      success_callback_.Reset();
    }
  }

 private:
  AccessCodeCallback success_callback_;
  ErrorCallback error_callback_;
};

// Will invoke the given `callback` if the host launch is completed, either
// successfully or failure.
class HostLaunchObserver : public CrdSessionObserver {
 public:
  explicit HostLaunchObserver(base::OnceClosure launch_done)
      : launch_done_(std::move(launch_done)) {}

  HostLaunchObserver(const HostLaunchObserver&) = delete;
  HostLaunchObserver& operator=(const HostLaunchObserver&) = delete;
  ~HostLaunchObserver() override = default;

  // `CrdSessionObserver` implementation:
  void OnHostStarted() override {
    if (launch_done_) {
      std::move(launch_done_).Run();
    }
  }

  void OnHostStopped(ExtendedStartCrdSessionResultCode,
                     const std::string&) override {
    // If we come here before `OnHostStarted()` it means the launch failed.
    if (launch_done_) {
      std::move(launch_done_).Run();
    }
  }

 private:
  base::OnceClosure launch_done_;
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
  std::optional<base::Time> session_connected_time_;
};

// Rejects incoming sessions when there is more than 15 minutes between
// generating and using the access code.
// We should not need this since the server side already enforces a TTL of 5
// minutes (at the time of writing), but we add this as a stopgap just in case a
// malicious admin finds a way around the server side protection.
class AccessCodeTtlChecker : public CrdSessionObserver {
 public:
  explicit AccessCodeTtlChecker(base::OnceClosure terminate_session_callback)
      : terminate_session_(std::move(terminate_session_callback)) {}

  AccessCodeTtlChecker(const AccessCodeTtlChecker&) = delete;
  AccessCodeTtlChecker& operator=(const AccessCodeTtlChecker&) = delete;
  ~AccessCodeTtlChecker() override = default;

  // `CrdSessionObserver` implementation:
  void OnAccessCodeReceived(const std::string&) override {
    terminate_timer_.emplace();
    terminate_timer_->Start(
        FROM_HERE, kAccessCodeMaxTTL,
        base::BindOnce([]() {
          CRD_LOG(WARNING)
              << "Terminating CRD Host since access code outlived its TTL";
        }).Then(std::move(terminate_session_)));
  }

  void OnClientConnected() override { terminate_timer_.reset(); }

 private:
  base::OnceClosure terminate_session_;
  std::optional<base::OneShotTimer> terminate_timer_;
};

remoting::mojom::SupportSessionParamsPtr GetSessionParameters(
    const SessionParameters& parameters) {
  auto result = remoting::mojom::SupportSessionParams::New();
  result->user_name = parameters.user_name;
  result->authorized_helper = parameters.admin_email;
  result->oauth_access_token = parameters.oauth_token;

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

DeviceOAuth2TokenService* GetOAuthService() {
  return DeviceOAuth2TokenServiceFactory::Get();
}

std::unique_ptr<CrdOAuthTokenFetcher> CreateOAuthTokenFetcher(
    DeviceOAuth2TokenService* service,
    std::optional<std::string> oauth_token_for_test) {
  if (service) {
    return std::make_unique<RealCrdOAuthTokenFetcher>(CHECK_DEREF(service));
  } else {
    CHECK_IS_TEST();
    return std::make_unique<FakeCrdOAuthTokenFetcher>(oauth_token_for_test);
  }
}

}  // namespace

// Base class for classes responsible to launch a `CrdHostSession`.
// Derived classes must implement `Launch()` and then report success or failure
// to launch through the `ReportLaunchSuccess()` and `ReportLaunchFailure()`
// methods.
class CrdAdminSessionController::SessionLauncher {
 public:
  using SessionLaunchedCallback = base::OnceCallback<void(SessionLaunchResult)>;

  virtual ~SessionLauncher() = default;

  virtual void Launch(SessionLaunchedCallback on_session_launched) = 0;
};

class CrdAdminSessionController::CrdHostSession {
 public:
  CrdHostSession() = default;
  CrdHostSession(const CrdHostSession&) = delete;
  CrdHostSession& operator=(const CrdHostSession&) = delete;
  ~CrdHostSession() = default;

  // Runs the given launcher to start the CRD host session.
  // All results are reported through the observers.
  void Launch(std::unique_ptr<SessionLauncher> launcher) {
    launcher_ = std::move(launcher);
    launcher_->Launch(
        base::BindOnce(&CrdHostSession::OnLaunchDone, base::Unretained(this)));
  }

  bool IsSessionCurtained() const { return is_curtained_; }

  void AddObserver(CrdSessionObserver* observer) {
    observer_proxy_.AddObserver(observer);
  }

  void AddOwnedObserver(std::unique_ptr<CrdSessionObserver> observer) {
    observer_proxy_.AddOwnedObserver(std::move(observer));
  }

 private:
  void OnLaunchDone(SessionLaunchResult result) {
    if (result.has_value()) {
      Start(std::move(result).value());
    } else {
      // Inform observers of the launch failure.
      observer_proxy_.ReportHostStopped(result.error(), "");
    }

    // Cleanup the launcher, but do it asynchronously since it's the launcher
    // that's invoking this `OnLaunchDone` method.
    DeleteSoon(std::move(launcher_));
  }

  void Start(SessionStartParameters parameters) {
    CRD_DVLOG(1) << "CRD Host session started successfully";
    is_curtained_ = parameters.curtained;
    observer_proxy_.Bind(std::move(std::move(parameters.host_observer)));
  }

  SupportHostObserverProxy observer_proxy_;
  std::unique_ptr<SessionLauncher> launcher_;
  bool is_curtained_ = false;
};

// Launcher that starts a new CRD session.
class CrdAdminSessionController::NewSessionLauncher : public SessionLauncher {
 public:
  NewSessionLauncher(RemotingServiceProxy& remoting_service,
                     const SessionParameters& parameters)
      : remoting_service_(remoting_service), parameters_(parameters) {}

  void Launch(SessionLaunchedCallback on_session_launched) override {
    on_session_launched_ = std::move(on_session_launched);
    Start();
  }

 private:
  void Start() {
    CRD_DVLOG(3) << "Starting CRD session with parameters " << parameters_;
    remoting_service_->StartSession(
        GetSessionParameters(parameters_), GetEnterpriseParameters(parameters_),
        base::BindOnce(&NewSessionLauncher::OnSessionStartResponse,
                       weak_factory_.GetWeakPtr()));
  }

  void OnSessionStartResponse(
      remoting::mojom::StartSupportSessionResponsePtr response) {
    if (response->is_support_session_error()) {
      ReportLaunchFailure(
          ExtendedStartCrdSessionResultCode::kFailureCrdHostError);
      return;
    }

    ReportLaunchSuccess({.curtained = parameters_.curtain_local_user_session,
                         .host_observer = std::move(response->get_observer())});
  }

  void ReportLaunchSuccess(SessionStartParameters parameters) {
    std::move(on_session_launched_).Run(std::move(parameters));
  }

  void ReportLaunchFailure(ExtendedStartCrdSessionResultCode error) {
    std::move(on_session_launched_).Run(base::unexpected(error));
  }

  SessionLaunchedCallback on_session_launched_;
  raw_ref<RemotingServiceProxy> remoting_service_;
  const SessionParameters parameters_;

  base::WeakPtrFactory<NewSessionLauncher> weak_factory_{this};
};

// Launcher that checks if a reconnectable CRD session is available, and if so
// connects to it.
class CrdAdminSessionController::ReconnectedSessionLauncher
    : public SessionLauncher {
 public:
  ReconnectedSessionLauncher(
      RemotingServiceProxy& remoting_service,
      std::unique_ptr<CrdOAuthTokenFetcher> oauth_token_fetcher)
      : remoting_service_(remoting_service),
        oauth_token_fetcher_(std::move(oauth_token_fetcher)) {}

  void Launch(SessionLaunchedCallback on_session_launched) override {
    on_session_launched_ = std::move(on_session_launched);
    TryToReconnect();
  }

 private:
  void TryToReconnect() {
    CRD_DVLOG(3) << "Checking for reconnectable session";

    // First fetch the id of the reconnectable session.
    remoting_service_->GetReconnectableSessionId(base::BindOnce(
        // Then fetch a new OAuth token.
        &ReconnectedSessionLauncher::FetchOAuthToken,
        weak_factory_.GetWeakPtr(),
        base::BindOnce(
            // And finally reconnect to the session.
            &ReconnectedSessionLauncher::ReconnectToSession,
            weak_factory_.GetWeakPtr())));
  }

  void FetchOAuthToken(
      base::OnceCallback<void(remoting::SessionId, std::optional<std::string>)>
          on_done,
      std::optional<remoting::SessionId> id) {
    if (!id.has_value()) {
      CRD_DVLOG(3) << "No reconnectable CRD session found.";
      return ReportLaunchFailure(
          ExtendedStartCrdSessionResultCode::kFailureUnknownError);
    }

    CHECK_DEREF(oauth_token_fetcher_.get())
        .Start(base::BindOnce(std::move(on_done), id.value()));
  }

  void ReconnectToSession(remoting::SessionId id,
                          std::optional<std::string> oauth_token) {
    CRD_LOG(INFO) << "Attempting to resume reconnectable session";

    if (!oauth_token.has_value()) {
      CRD_LOG(WARNING)
          << "Failed to fetch OAuth token for reconnectable session";
      return ReportLaunchFailure(
          ExtendedStartCrdSessionResultCode::kFailureNoOauthToken);
    }

    remoting_service_->ReconnectToSession(
        id, oauth_token.value(),
        base::BindOnce(&ReconnectedSessionLauncher::OnSessionStartResponse,
                       weak_factory_.GetWeakPtr()));
  }

  void OnSessionStartResponse(
      remoting::mojom::StartSupportSessionResponsePtr response) {
    if (response->is_support_session_error()) {
      return ReportLaunchFailure(
          ExtendedStartCrdSessionResultCode::kFailureCrdHostError);
    }

    ReportLaunchSuccess({.curtained = true,
                         .host_observer = std::move(response->get_observer())});
  }

  void ReportLaunchSuccess(SessionStartParameters parameters) {
    std::move(on_session_launched_).Run(std::move(parameters));
  }

  void ReportLaunchFailure(ExtendedStartCrdSessionResultCode error) {
    std::move(on_session_launched_).Run(base::unexpected(error));
  }

  SessionLaunchedCallback on_session_launched_;
  raw_ref<RemotingServiceProxy> remoting_service_;
  std::unique_ptr<CrdOAuthTokenFetcher> oauth_token_fetcher_;

  base::WeakPtrFactory<ReconnectedSessionLauncher> weak_factory_{this};
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
    CHECK(!notification_controller_);
    notification_controller_ =
        std::make_unique<RemoteActivityNotificationController>(
            CHECK_DEREF(local_state),
            base::BindRepeating(
                &CrdAdminSessionController::IsCurrentSessionCurtained,
                base::Unretained(this)));

    TryToReconnect(std::move(done_callback));
  } else {
    std::move(done_callback).Run();
  }
}

void CrdAdminSessionController::Shutdown() {
  notification_controller_ = nullptr;
  TerminateSession();
}

void CrdAdminSessionController::SetOAuthTokenForTesting(
    std::string_view token) {
  CHECK_IS_TEST();
  oauth_token_for_test_ = token;
}

void CrdAdminSessionController::ClearOAuthTokenForTesting() {
  CHECK_IS_TEST();
  oauth_token_for_test_.reset();
}

StartCrdSessionJobDelegate& CrdAdminSessionController::GetDelegate() {
  return *this;
}

bool CrdAdminSessionController::HasActiveSession() const {
  return active_session_ != nullptr;
}

void CrdAdminSessionController::TerminateSession() {
  CRD_DVLOG(3) << "Terminating CRD session";
  active_session_ = nullptr;
}

void CrdAdminSessionController::OnHostStopped(
    ExtendedStartCrdSessionResultCode result,
    const std::string& message) {
  CRD_DVLOG(3) << "Destroying CRD host session asynchronously";

  DeleteSoon(std::move(active_session_));
}

void CrdAdminSessionController::TryToReconnect(
    base::OnceClosure done_callback) {
  CHECK(!HasActiveSession());

  active_session_ = CreateCrdHostSession();
  active_session_->AddOwnedObserver(
      std::make_unique<HostLaunchObserver>(std::move(done_callback)));

  active_session_->Launch(std::make_unique<ReconnectedSessionLauncher>(
      *remoting_service_,
      CreateOAuthTokenFetcher(GetOAuthService(), oauth_token_for_test_)));
}

void CrdAdminSessionController::StartCrdHostAndGetCode(
    const SessionParameters& parameters,
    AccessCodeCallback success_callback,
    ErrorCallback error_callback,
    SessionEndCallback session_finished_callback) {
  CHECK(!HasActiveSession());

  CRD_DVLOG(3) << "Starting CRD host session";

  active_session_ = CreateCrdHostSession();

  active_session_->AddOwnedObserver(std::make_unique<AccessCodeObserver>(
      std::move(success_callback), std::move(error_callback)));
  active_session_->AddOwnedObserver(std::make_unique<SessionDurationObserver>(
      std::move(session_finished_callback)));

  active_session_->Launch(
      std::make_unique<NewSessionLauncher>(*remoting_service_, parameters));
}

std::unique_ptr<CrdAdminSessionController::CrdHostSession>
CrdAdminSessionController::CreateCrdHostSession() {
  auto result = std::make_unique<CrdHostSession>();

  result->AddOwnedObserver(std::make_unique<AccessCodeTtlChecker>(
      base::BindOnce(&CrdAdminSessionController::TerminateSession,
                     base::Unretained(this))));
  result->AddObserver(this);

  if (base::FeatureList::IsEnabled(kEnableCrdAdminRemoteAccessV2)) {
    CHECK(notification_controller_);
    result->AddObserver(notification_controller_.get());
  }

  return result;
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
