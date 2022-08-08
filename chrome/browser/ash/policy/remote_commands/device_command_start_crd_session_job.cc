// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "remoting/host/chromeos/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

// OAuth2 Token scopes
constexpr char kCloudDevicesOAuth2Scope[] =
    "https://www.googleapis.com/auth/clouddevices";
constexpr char kChromotingRemoteSupportOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromoting.remote.support";
constexpr char kTachyonOAuth2Scope[] =
    "https://www.googleapis.com/auth/tachyon";

// Job parameters fields:

// Job requires that UI was idle for at least this period of time
// to proceed. If absent / equal to 0, job will proceed regardless of user
// activity.
const char kIdlenessCutoffFieldName[] = "idlenessCutoffSec";

// True if the admin has confirmed that they want to start the CRD session
// while a user is currently using the device.
const char kAckedUserPresenceFieldName[] = "ackedUserPresence";

// True if the admin wants to start a private remote access session where the
// physical displays are curtained off so the local user can not see what the
// admin is doing.
const char kCurtainLocalUserSession[] = "curtainLocalUserSession";

// Result payload fields:

// Integer value containing DeviceCommandStartCrdSessionJob::ResultCode
const char kResultCodeFieldName[] = "resultCode";

// CRD Access Code if job was completed successfully
const char kResultAccessCodeFieldName[] = "accessCode";

// Optional detailed error message for error result codes.
const char kResultMessageFieldName[] = "message";

// Period in seconds since last user activity, if job finished with
// FAILURE_NOT_IDLE result code.
const char kResultLastActivityFieldName[] = "lastActivitySec";

ash::KioskAppManagerBase* GetKioskAppManagerIfKioskAppIsRunning(
    const user_manager::UserManager* user_manager) {
  if (user_manager->IsLoggedInAsKioskApp())
    return ash::KioskAppManager::Get();
  if (user_manager->IsLoggedInAsArcKioskApp())
    return chromeos::ArcKioskAppManager::Get();
  if (user_manager->IsLoggedInAsWebKioskApp())
    return ash::WebKioskAppManager::Get();

  return nullptr;
}

void SendResultCodeToUma(
    DeviceCommandStartCrdSessionJob::ResultCode result_code) {
  base::UmaHistogramEnumeration("Enterprise.DeviceRemoteCommand.Crd.Result",
                                result_code);
}

void SendSessionTypeToUma(
    DeviceCommandStartCrdSessionJob::UmaSessionType session_type) {
  base::UmaHistogramEnumeration(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType", session_type);
}

}  // namespace

// Helper class that asynchronously fetches the OAuth token, and passes it to
// the given callback.
class DeviceCommandStartCrdSessionJob::OAuthTokenFetcher
    : public OAuth2AccessTokenManager::Consumer {
 public:
  OAuthTokenFetcher(DeviceOAuth2TokenService& oauth_service,
                    absl::optional<std::string> oauth_token_for_test,
                    OAuthTokenCallback success_callback,
                    ErrorCallback error_callback)
      : OAuth2AccessTokenManager::Consumer("crd_host_delegate"),
        oauth_service_(oauth_service),
        oauth_token_for_test_(std::move(oauth_token_for_test)),
        success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)) {}
  OAuthTokenFetcher(const OAuthTokenFetcher&) = delete;
  OAuthTokenFetcher& operator=(const OAuthTokenFetcher&) = delete;
  ~OAuthTokenFetcher() override = default;

  void Start() {
    CRD_DVLOG(1) << "Fetching OAuth access token";

    if (oauth_token_for_test_) {
      std::move(success_callback_).Run(oauth_token_for_test_.value());
      return;
    }

    OAuth2AccessTokenManager::ScopeSet scopes{
        GaiaConstants::kGoogleUserInfoEmail, kCloudDevicesOAuth2Scope,
        kChromotingRemoteSupportOAuth2Scope, kTachyonOAuth2Scope};
    oauth_request_ = oauth_service_.StartAccessTokenRequest(scopes, this);
  }

  bool is_running() const { return oauth_request_ != nullptr; }

 private:
  // OAuth2AccessTokenManager::Consumer implementation:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    CRD_DVLOG(1) << "Received OAuth access token";
    oauth_request_.reset();
    std::move(success_callback_).Run(token_response.access_token);
  }

  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override {
    CRD_DVLOG(1) << "Failed to get OAuth access token: " << error.ToString();
    oauth_request_.reset();
    std::move(error_callback_)
        .Run(DeviceCommandStartCrdSessionJob::FAILURE_NO_OAUTH_TOKEN,
             error.ToString());
  }

  DeviceOAuth2TokenService& oauth_service_;
  absl::optional<std::string> oauth_token_for_test_;
  DeviceCommandStartCrdSessionJob::OAuthTokenCallback success_callback_;
  DeviceCommandStartCrdSessionJob::ErrorCallback error_callback_;
  // Handler for the OAuth access token request.
  // When deleted the token manager will cancel the request (and not call us).
  std::unique_ptr<OAuth2AccessTokenManager::Request> oauth_request_;
};

class DeviceCommandStartCrdSessionJob::ResultPayload
    : public RemoteCommandJob::ResultPayload {
 public:
  ResultPayload(ResultCode result_code,
                const absl::optional<std::string>& access_code,
                const absl::optional<base::TimeDelta>& time_delta,
                const absl::optional<std::string>& error_message);
  ~ResultPayload() override = default;

  static std::unique_ptr<ResultPayload> CreateSuccessPayload(
      const std::string& access_code);
  static std::unique_ptr<ResultPayload> CreateNonIdlePayload(
      const base::TimeDelta& time_delta);
  static std::unique_ptr<ResultPayload> CreateErrorPayload(
      ResultCode result_code,
      const std::string& error_message);

  // RemoteCommandJob::ResultPayload:
  std::unique_ptr<std::string> Serialize() override;

 private:
  std::string payload_;
};

DeviceCommandStartCrdSessionJob::ResultPayload::ResultPayload(
    ResultCode result_code,
    const absl::optional<std::string>& access_code,
    const absl::optional<base::TimeDelta>& time_delta,
    const absl::optional<std::string>& error_message) {
  base::Value::Dict value;
  value.Set(kResultCodeFieldName, result_code);
  if (error_message && !error_message.value().empty())
    value.Set(kResultMessageFieldName, error_message.value());
  if (access_code)
    value.Set(kResultAccessCodeFieldName, access_code.value());
  if (time_delta) {
    value.Set(kResultLastActivityFieldName,
              static_cast<int>(time_delta.value().InSeconds()));
  }
  base::JSONWriter::Write(value, &payload_);
}

std::unique_ptr<DeviceCommandStartCrdSessionJob::ResultPayload>
DeviceCommandStartCrdSessionJob::ResultPayload::CreateSuccessPayload(
    const std::string& access_code) {
  return std::make_unique<ResultPayload>(ResultCode::SUCCESS, access_code,
                                         /*time_delta=*/absl::nullopt,
                                         /*error_message=*/absl::nullopt);
}

std::unique_ptr<DeviceCommandStartCrdSessionJob::ResultPayload>
DeviceCommandStartCrdSessionJob::ResultPayload::CreateNonIdlePayload(
    const base::TimeDelta& time_delta) {
  return std::make_unique<ResultPayload>(
      ResultCode::FAILURE_NOT_IDLE, /*access_code=*/absl::nullopt, time_delta,
      /*error_message=*/absl::nullopt);
}

std::unique_ptr<DeviceCommandStartCrdSessionJob::ResultPayload>
DeviceCommandStartCrdSessionJob::ResultPayload::CreateErrorPayload(
    ResultCode result_code,
    const std::string& error_message) {
  DCHECK(result_code != ResultCode::SUCCESS);
  DCHECK(result_code != ResultCode::FAILURE_NOT_IDLE);
  return std::make_unique<ResultPayload>(
      result_code, /*access_code=*/absl::nullopt,
      /*time_delta=*/absl::nullopt, error_message);
}

std::unique_ptr<std::string>
DeviceCommandStartCrdSessionJob::ResultPayload::Serialize() {
  return std::make_unique<std::string>(payload_);
}

DeviceCommandStartCrdSessionJob::DeviceCommandStartCrdSessionJob(
    Delegate* crd_host_delegate)
    : delegate_(crd_host_delegate) {
  DCHECK(crd_host_delegate);
}

DeviceCommandStartCrdSessionJob::~DeviceCommandStartCrdSessionJob() = default;

enterprise_management::RemoteCommand_Type
DeviceCommandStartCrdSessionJob::GetType() const {
  return enterprise_management::RemoteCommand_Type_DEVICE_START_CRD_SESSION;
}

void DeviceCommandStartCrdSessionJob::SetOAuthTokenForTest(
    const std::string& token) {
  oauth_token_for_test_ = token;
}

bool DeviceCommandStartCrdSessionJob::ParseCommandPayload(
    const std::string& command_payload) {
  absl::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root)
    return false;
  if (!root->is_dict())
    return false;

  idleness_cutoff_ =
      base::Seconds(root->FindIntKey(kIdlenessCutoffFieldName).value_or(0));

  acked_user_presence_ =
      root->FindBoolKey(kAckedUserPresenceFieldName).value_or(false);

  curtain_local_user_session_ =
      root->FindBoolKey(kCurtainLocalUserSession).value_or(false);

  if (base::FeatureList::IsEnabled(
          remoting::features::kForceCrdAdminRemoteAccess)) {
    CRD_LOG(WARNING) << "Forcing remote access";
    curtain_local_user_session_ = true;
  }

  if (curtain_local_user_session_ &&
      !base::FeatureList::IsEnabled(
          remoting::features::kEnableCrdAdminRemoteAccess)) {
    LOG(WARNING)
        << "Rejecting curtain_local_user_session as CRD remote access feature "
           "is not enabled";
    return false;
  }

  return true;
}

void DeviceCommandStartCrdSessionJob::RunImpl(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback) {
  CRD_LOG(INFO) << "Running start CRD session command";

  if (delegate_->HasActiveSession()) {
    CHECK(!terminate_session_attempted_);
    terminate_session_attempted_ = true;

    CRD_DVLOG(1) << "Terminating active session";
    delegate_->TerminateSession(base::BindOnce(
        &DeviceCommandStartCrdSessionJob::RunImpl, weak_factory_.GetWeakPtr(),
        std::move(succeeded_callback), std::move(failed_callback)));
    return;
  }
  terminate_session_attempted_ = false;

  failed_callback_ = std::move(failed_callback);
  succeeded_callback_ = std::move(succeeded_callback);

  if (!AreServicesReady()) {
    FinishWithError(ResultCode::FAILURE_SERVICES_NOT_READY, "");
    return;
  }

  if (!UserTypeSupportsCrd()) {
    FinishWithError(ResultCode::FAILURE_UNSUPPORTED_USER_TYPE, "");
    return;
  }

  if (!IsDeviceIdle()) {
    FinishWithNotIdleError();
    return;
  }

  FetchOAuthTokenASync(
      /*on_success=*/base::BindOnce(
          &DeviceCommandStartCrdSessionJob::StartCrdHostAndGetCode,
          weak_factory_.GetWeakPtr()),
      /*on_error=*/base::BindOnce(
          &DeviceCommandStartCrdSessionJob::FinishWithError,
          weak_factory_.GetWeakPtr()));
}

void DeviceCommandStartCrdSessionJob::FetchOAuthTokenASync(
    OAuthTokenCallback on_success,
    ErrorCallback on_error) {
  DCHECK(!oauth_token_fetcher_ || !oauth_token_fetcher_->is_running());
  DCHECK(oauth_service());

  oauth_token_fetcher_ = std::make_unique<OAuthTokenFetcher>(
      *oauth_service(), std::move(oauth_token_for_test_), std::move(on_success),
      std::move(on_error));
  oauth_token_fetcher_->Start();
}

void DeviceCommandStartCrdSessionJob::StartCrdHostAndGetCode(
    const std::string& token) {
  CRD_DVLOG(1) << "Received OAuth token, now retrieving CRD access code";
  Delegate::SessionParameters parameters{
      .oauth_token = token,
      .user_name = GetRobotAccountUserName(),
      .terminate_upon_input = ShouldTerminateUponInput(),
      .show_confirmation_dialog = ShouldShowConfirmationDialog(),
      .curtain_local_user_session = curtain_local_user_session_};
  delegate_->StartCrdHostAndGetCode(
      parameters,
      base::BindOnce(&DeviceCommandStartCrdSessionJob::FinishWithSuccess,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&DeviceCommandStartCrdSessionJob::FinishWithError,
                     weak_factory_.GetWeakPtr()));
}

void DeviceCommandStartCrdSessionJob::FinishWithSuccess(
    const std::string& access_code) {
  CRD_LOG(INFO) << "Successfully received CRD access code";
  if (!succeeded_callback_)
    return;  // Task was terminated.

  SendResultCodeToUma(ResultCode::SUCCESS);
  SendSessionTypeToUma(GetUmaSessionType());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(succeeded_callback_),
                     ResultPayload::CreateSuccessPayload(access_code)));
}

void DeviceCommandStartCrdSessionJob::FinishWithError(
    const ResultCode result_code,
    const std::string& message) {
  DCHECK(result_code != ResultCode::SUCCESS);
  CRD_LOG(INFO) << "Not starting CRD session because of error (code "
                << result_code << ", message '" << message << "')";
  if (!failed_callback_)
    return;  // Task was terminated.

  SendResultCodeToUma(result_code);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(failed_callback_),
                     ResultPayload::CreateErrorPayload(result_code, message)));
}

void DeviceCommandStartCrdSessionJob::FinishWithNotIdleError() {
  CRD_LOG(INFO) << "Not starting CRD session because device is not idle";
  if (!failed_callback_)
    return;  // Task was terminated.

  SendResultCodeToUma(ResultCode::FAILURE_NOT_IDLE);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(failed_callback_),
                                ResultPayload::CreateNonIdlePayload(
                                    GetDeviceIdlenessPeriod())));
}

bool DeviceCommandStartCrdSessionJob::AreServicesReady() const {
  return user_manager::UserManager::IsInitialized() &&
         ui::UserActivityDetector::Get() != nullptr &&
         oauth_service() != nullptr;
}

bool DeviceCommandStartCrdSessionJob::UserTypeSupportsCrd() const {
  const UserType current_user_type = GetUserType();

  CRD_DVLOG(2) << "User is of type " << UserTypeToString(current_user_type);

  if (curtain_local_user_session_) {
    return current_user_type == UserType::kNoUser;
  }

  switch (current_user_type) {
    case UserType::kAffiliatedUser:
    case UserType::kAutoLaunchedKiosk:
    case UserType::kManagedGuestSession:
    case UserType::kManuallyLaunchedKiosk:
      return true;
    case UserType::kNoUser:
    case UserType::kOther:
      return false;
  }
  NOTREACHED();
  return false;
}

DeviceCommandStartCrdSessionJob::UserType
DeviceCommandStartCrdSessionJob::GetUserType() const {
  const auto* user_manager = user_manager::UserManager::Get();

  if (!user_manager->IsUserLoggedIn())
    return UserType::kNoUser;

  if (user_manager->IsLoggedInAsAnyKioskApp()) {
    if (IsRunningAutoLaunchedKiosk())
      return UserType::kAutoLaunchedKiosk;
    else
      return UserType::kManuallyLaunchedKiosk;
  }

  if (user_manager->IsLoggedInAsPublicAccount())
    return UserType::kManagedGuestSession;

  if (user_manager->GetActiveUser()->IsAffiliated())
    return UserType::kAffiliatedUser;

  return UserType::kOther;
}

DeviceCommandStartCrdSessionJob::UmaSessionType
DeviceCommandStartCrdSessionJob::GetUmaSessionType() const {
  switch (GetUserType()) {
    case UserType::kAutoLaunchedKiosk:
      return UmaSessionType::kAutoLaunchedKiosk;
    case UserType::kAffiliatedUser:
      return UmaSessionType::kAffiliatedUser;
    case UserType::kManagedGuestSession:
      return UmaSessionType::kManagedGuestSession;
    case UserType::kManuallyLaunchedKiosk:
      return UmaSessionType::kManuallyLaunchedKiosk;
    case UserType::kNoUser:
      // TODO(b/236689277): Introduce UmaSessionType::kNoLocalUser.
      return UmaSessionType::kMaxValue;
    case UserType::kOther:
      NOTREACHED();
      return UmaSessionType::kMaxValue;
  }
}

bool DeviceCommandStartCrdSessionJob::IsRunningAutoLaunchedKiosk() const {
  const auto* user_manager = user_manager::UserManager::Get();
  const auto* kiosk_app_manager =
      GetKioskAppManagerIfKioskAppIsRunning(user_manager);

  if (!kiosk_app_manager)
    return false;
  return kiosk_app_manager->current_app_was_auto_launched_with_zero_delay();
}

bool DeviceCommandStartCrdSessionJob::IsDeviceIdle() const {
  return GetDeviceIdlenessPeriod() >= idleness_cutoff_;
}

base::TimeDelta DeviceCommandStartCrdSessionJob::GetDeviceIdlenessPeriod()
    const {
  return base::TimeTicks::Now() -
         ui::UserActivityDetector::Get()->last_activity_time();
}

std::string DeviceCommandStartCrdSessionJob::GetRobotAccountUserName() const {
  CoreAccountId account_id = oauth_service()->GetRobotAccountId();

  // TODO(msarda): This conversion will not be correct once account id is
  // migrated to be the Gaia ID on ChromeOS. Fix it.
  return account_id.ToString();
}

bool DeviceCommandStartCrdSessionJob::ShouldShowConfirmationDialog() const {
  switch (GetUserType()) {
    case UserType::kAffiliatedUser:
    case UserType::kManagedGuestSession:
      return true;
    case UserType::kAutoLaunchedKiosk:
    case UserType::kManuallyLaunchedKiosk:
    case UserType::kNoUser:
    case UserType::kOther:
      return false;
  }
  NOTREACHED();
  return false;
}

bool DeviceCommandStartCrdSessionJob::ShouldTerminateUponInput() const {
  if (curtain_local_user_session_)
    return false;

  switch (GetUserType()) {
    case UserType::kAffiliatedUser:
    case UserType::kManagedGuestSession:
      // We never terminate upon input for the user-session scenarios, because:
      //   1. There is no risk of the admin spying on the users, as they need to
      //       explicitly accept the connection request.
      //   2. If we terminate upon input the session will immediately be
      //      terminated as soon as the user accepts the connection request,
      //      as pressing the button to accept the connection request counts as
      //      user input.
      return false;
    case UserType::kAutoLaunchedKiosk:
    case UserType::kManuallyLaunchedKiosk:
      return !acked_user_presence_;
    case UserType::kNoUser:
    case UserType::kOther:
      // This method will only be called for user types for which we support
      // CRD sessions.
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

DeviceOAuth2TokenService* DeviceCommandStartCrdSessionJob::oauth_service()
    const {
  return DeviceOAuth2TokenServiceFactory::Get();
}

void DeviceCommandStartCrdSessionJob::TerminateImpl() {
  succeeded_callback_.Reset();
  failed_callback_.Reset();
  weak_factory_.InvalidateWeakPtrs();
  delegate_->TerminateSession(base::OnceClosure());
}

const char* DeviceCommandStartCrdSessionJob::UserTypeToString(
    UserType value) const {
  switch (value) {
    case UserType::kAutoLaunchedKiosk:
      return "kAutoLaunchedKiosk";
    case UserType::kManuallyLaunchedKiosk:
      return "kManuallyLaunchedKiosk";
    case UserType::kNoUser:
      return "kNoUser";
    case UserType::kAffiliatedUser:
      return "kAffiliatedUser";
    case UserType::kManagedGuestSession:
      return "kManagedGuestSession";
    case UserType::kOther:
      return "kOther";
  }
  NOTREACHED();
  return "<invalid user type>";
}

}  // namespace policy
