// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"

#include <iomanip>
#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/crd_uma_logger.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "remoting/host/chromeos/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace {

using SessionParameters = StartCrdSessionJobDelegate::SessionParameters;
using ErrorCallback = StartCrdSessionJobDelegate::ErrorCallback;

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

// The type of CRD session that the admin wants to start.
const char kCrdSessionTypeFieldName[] = "crdSessionType";

// The admin's email address.
const char kAdminEmailFieldName[] = "adminEmail";

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

absl::optional<std::string> FindString(const base::Value::Dict& dict,
                                       base::StringPiece key) {
  if (!dict.contains(key)) {
    return absl::nullopt;
  }
  return *dict.FindString(key);
}

void SendResultCodeToUma(CrdSessionType crd_session_type,
                         UserSessionType user_session_type,
                         ResultCode result_code) {
  base::UmaHistogramEnumeration("Enterprise.DeviceRemoteCommand.Crd.Result",
                                result_code);

  CrdUmaLogger(crd_session_type, user_session_type)
      .LogSessionLaunchResult(result_code);
}

void SendSessionTypeToUma(
    DeviceCommandStartCrdSessionJob::UmaSessionType session_type) {
  base::UmaHistogramEnumeration(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType", session_type);
}

std::string CreateSuccessPayload(const std::string& access_code) {
  return base::WriteJson(base::Value::Dict()
                             .Set(kResultCodeFieldName,
                                  static_cast<int>(ResultCode::SUCCESS))
                             .Set(kResultAccessCodeFieldName, access_code))
      .value();
}

std::string CreateNonIdlePayload(const base::TimeDelta& time_delta) {
  return base::WriteJson(
             base::Value::Dict()
                 .Set(kResultCodeFieldName,
                      static_cast<int>(ResultCode::FAILURE_NOT_IDLE))
                 .Set(kResultLastActivityFieldName,
                      static_cast<int>(time_delta.InSeconds())))
      .value();
}

std::string CreateErrorPayload(ResultCode result_code,
                               const std::string& error_message) {
  DCHECK(result_code != ResultCode::SUCCESS);
  DCHECK(result_code != ResultCode::FAILURE_NOT_IDLE);

  auto payload = base::Value::Dict()  //
                     .Set(kResultCodeFieldName, static_cast<int>(result_code));
  if (!error_message.empty()) {
    payload.Set(kResultMessageFieldName, error_message);
  }
  return base::WriteJson(payload).value();
}

DeviceOAuth2TokenService& GetOAuthService() {
  return CHECK_DEREF(DeviceOAuth2TokenServiceFactory::Get());
}

CrdSessionType ToCrdSessionTypeOrDefault(absl::optional<int> int_value,
                                         CrdSessionType default_value) {
  if (!int_value.has_value() ||
      !enterprise_management::CrdSessionType_IsValid(int_value.value())) {
    return default_value;
  }
  return static_cast<CrdSessionType>(int_value.value());
}

void OnCrdSessionFinished(CrdSessionType crd_session_type,
                          UserSessionType user_session_type,
                          base::TimeDelta session_duration) {
  CrdUmaLogger(crd_session_type, user_session_type)
      .LogSessionDuration(session_duration);
}

bool IsKioskSession(UserSessionType session_type) {
  return session_type == UserSessionType::AUTO_LAUNCHED_KIOSK_SESSION ||
         session_type == UserSessionType::MANUALLY_LAUNCHED_KIOSK_SESSION;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OAuthTokenFetcher
////////////////////////////////////////////////////////////////////////////////

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
    oauth_request_ = oauth_service_->StartAccessTokenRequest(scopes, this);
  }

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
        .Run(ResultCode::FAILURE_NO_OAUTH_TOKEN, error.ToString());
  }

  const raw_ref<DeviceOAuth2TokenService, ExperimentalAsh> oauth_service_;
  absl::optional<std::string> oauth_token_for_test_;
  DeviceCommandStartCrdSessionJob::OAuthTokenCallback success_callback_;
  ErrorCallback error_callback_;
  // Handler for the OAuth access token request.
  // When deleted the token manager will cancel the request (and not call us).
  std::unique_ptr<OAuth2AccessTokenManager::Request> oauth_request_;
};

////////////////////////////////////////////////////////////////////////////////
// DeviceCommandStartCrdSessionJob
////////////////////////////////////////////////////////////////////////////////

DeviceCommandStartCrdSessionJob::DeviceCommandStartCrdSessionJob(
    Delegate& delegate)
    : delegate_(delegate) {}

DeviceCommandStartCrdSessionJob::~DeviceCommandStartCrdSessionJob() = default;

enterprise_management::RemoteCommand_Type
DeviceCommandStartCrdSessionJob::GetType() const {
  return enterprise_management::RemoteCommand::DEVICE_START_CRD_SESSION;
}

void DeviceCommandStartCrdSessionJob::SetOAuthTokenForTest(
    const std::string& token) {
  oauth_token_for_test_ = token;
}

bool DeviceCommandStartCrdSessionJob::ParseCommandPayload(
    const std::string& command_payload) {
  absl::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root || !root->is_dict()) {
    LOG(WARNING) << "Rejecting remote command with invalid payload: "
                 << std::quoted(command_payload);
    return false;
  }
  CRD_DVLOG(1) << "Received remote command with payload "
               << std::quoted(command_payload);

  const base::Value::Dict& root_dict = root->GetDict();

  idleness_cutoff_ =
      base::Seconds(root_dict.FindInt(kIdlenessCutoffFieldName).value_or(0));

  acked_user_presence_ =
      root_dict.FindBool(kAckedUserPresenceFieldName).value_or(false);

  CrdSessionType crd_session_type =
      ToCrdSessionTypeOrDefault(root_dict.FindInt(kCrdSessionTypeFieldName),
                                CrdSessionType::REMOTE_SUPPORT_SESSION);

  admin_email_ = FindString(root_dict, kAdminEmailFieldName);

  curtain_local_user_session_ =
      (crd_session_type == CrdSessionType::REMOTE_ACCESS_SESSION);

  if (curtain_local_user_session_ &&
      !base::FeatureList::IsEnabled(
          remoting::features::kEnableCrdAdminRemoteAccess)) {
    LOG(WARNING) << "Rejecting CRD session type as CRD remote access feature "
                    "is not enabled";
    return false;
  }

  return true;
}

void DeviceCommandStartCrdSessionJob::RunImpl(
    CallbackWithResult result_callback) {
  CRD_LOG(INFO) << "Running start CRD session command";

  if (delegate_->HasActiveSession()) {
    CRD_DVLOG(1) << "Terminating active session";
    delegate_->TerminateSession();
    CHECK(!delegate_->HasActiveSession());
  }

  result_callback_ = std::move(result_callback);

  if (!UserTypeSupportsCrd()) {
    FinishWithError(ResultCode::FAILURE_UNSUPPORTED_USER_TYPE, "");
    return;
  }

  if (!IsDeviceIdle()) {
    FinishWithNotIdleError();
    return;
  }

  // First perform managed network check,
  CheckManagedNetworkASync(
      // Then fetch the OAuth token
      base::BindOnce(
          &DeviceCommandStartCrdSessionJob::FetchOAuthTokenASync,
          weak_factory_.GetWeakPtr(),
          // And finally start the CRD host.
          base::BindOnce(
              &DeviceCommandStartCrdSessionJob::StartCrdHostAndGetCode,
              weak_factory_.GetWeakPtr())));
}

void DeviceCommandStartCrdSessionJob::CheckManagedNetworkASync(
    base::OnceClosure on_success) {
  if (!curtain_local_user_session_) {
    // No need to check for managed networks if we are not going to curtain
    // off the local session.
    std::move(on_success).Run();
    return;
  }

  CalculateIsInManagedEnvironmentAsync(base::BindOnce(
      [](base::OnceClosure on_success, ErrorCallback on_error,
         bool is_in_managed_environment) {
        if (is_in_managed_environment) {
          std::move(on_success).Run();
        } else {
          std::move(on_error).Run(ResultCode::FAILURE_UNMANAGED_ENVIRONMENT,
                                  /*error_messages=*/"");
        }
      },
      std::move(on_success), GetErrorCallback()));
}

void DeviceCommandStartCrdSessionJob::FetchOAuthTokenASync(
    OAuthTokenCallback on_success) {
  DCHECK_EQ(oauth_token_fetcher_, nullptr);

  oauth_token_fetcher_ = std::make_unique<OAuthTokenFetcher>(
      GetOAuthService(), std::move(oauth_token_for_test_),
      std::move(on_success), GetErrorCallback());
  oauth_token_fetcher_->Start();
}

void DeviceCommandStartCrdSessionJob::StartCrdHostAndGetCode(
    const std::string& token) {
  CRD_DVLOG(1) << "Received OAuth token, now retrieving CRD access code";
  SessionParameters parameters;
  parameters.oauth_token = token;
  parameters.user_name = GetRobotAccountUserName();
  parameters.terminate_upon_input = ShouldTerminateUponInput();
  parameters.show_confirmation_dialog = ShouldShowConfirmationDialog();
  parameters.curtain_local_user_session = curtain_local_user_session_;
  parameters.admin_email = admin_email_;
  parameters.allow_troubleshooting_tools = ShouldAllowTroubleshootingTools();
  parameters.show_troubleshooting_tools = ShouldShowTroubleshootingTools();
  parameters.allow_reconnections = ShouldAllowReconnections();
  parameters.allow_file_transfer = ShouldAllowFileTransfer();

  delegate_->StartCrdHostAndGetCode(
      parameters,
      base::BindOnce(&DeviceCommandStartCrdSessionJob::FinishWithSuccess,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&DeviceCommandStartCrdSessionJob::FinishWithError,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&OnCrdSessionFinished, GetCrdSessionType(),
                     GetCurrentUserSessionType()));
}

void DeviceCommandStartCrdSessionJob::FinishWithSuccess(
    const std::string& access_code) {
  CRD_LOG(INFO) << "Successfully received CRD access code";
  if (!result_callback_) {
    return;  // Task was terminated.
  }

  SendResultCodeToUma(GetCrdSessionType(), GetCurrentUserSessionType(),
                      ResultCode::SUCCESS);
  SendSessionTypeToUma(GetUmaSessionType());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback_), ResultType::kSuccess,
                     CreateSuccessPayload(access_code)));
}

void DeviceCommandStartCrdSessionJob::FinishWithError(
    const ResultCode result_code,
    const std::string& message) {
  DCHECK(result_code != ResultCode::SUCCESS);
  CRD_LOG(INFO) << "Not starting CRD session because of error (code "
                << static_cast<int>(result_code) << ", message '" << message
                << "')";
  if (!result_callback_) {
    return;  // Task was terminated.
  }

  SendResultCodeToUma(GetCrdSessionType(), GetCurrentUserSessionType(),
                      result_code);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback_), ResultType::kFailure,
                     CreateErrorPayload(result_code, message)));
}

void DeviceCommandStartCrdSessionJob::FinishWithNotIdleError() {
  CRD_LOG(INFO) << "Not starting CRD session because device is not idle";
  if (!result_callback_) {
    return;  // Task was terminated.
  }

  SendResultCodeToUma(GetCrdSessionType(), GetCurrentUserSessionType(),
                      ResultCode::FAILURE_NOT_IDLE);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback_), ResultType::kFailure,
                     CreateNonIdlePayload(GetDeviceIdleTime())));
}

bool DeviceCommandStartCrdSessionJob::UserTypeSupportsCrd() const {
  CRD_DVLOG(2) << "User is of type "
               << UserSessionTypeToString(GetCurrentUserSessionType());

  if (curtain_local_user_session_) {
    return UserSessionSupportsRemoteAccess(GetCurrentUserSessionType());
  } else {
    return UserSessionSupportsRemoteSupport(GetCurrentUserSessionType());
  }
}

CrdSessionType DeviceCommandStartCrdSessionJob::GetCrdSessionType() const {
  if (curtain_local_user_session_) {
    return CrdSessionType::REMOTE_ACCESS_SESSION;
  }
  return CrdSessionType::REMOTE_SUPPORT_SESSION;
}

DeviceCommandStartCrdSessionJob::UmaSessionType
DeviceCommandStartCrdSessionJob::GetUmaSessionType() const {
  switch (GetCurrentUserSessionType()) {
    case UserSessionType::AUTO_LAUNCHED_KIOSK_SESSION:
      return UmaSessionType::kAutoLaunchedKiosk;
    case UserSessionType::AFFILIATED_USER_SESSION:
      return UmaSessionType::kAffiliatedUser;
    case UserSessionType::MANAGED_GUEST_SESSION:
      return UmaSessionType::kManagedGuestSession;
    case UserSessionType::MANUALLY_LAUNCHED_KIOSK_SESSION:
      return UmaSessionType::kManuallyLaunchedKiosk;
    case UserSessionType::NO_SESSION:
      // TODO(b/236689277): Introduce UmaSessionType::kNoLocalUser.
      return UmaSessionType::kMaxValue;
    case UserSessionType::UNAFFILIATED_USER_SESSION:
    case UserSessionType::GUEST_SESSION:
    case UserSessionType::USER_SESSION_TYPE_UNKNOWN:
      NOTREACHED();
      return UmaSessionType::kMaxValue;
  }
}

bool DeviceCommandStartCrdSessionJob::IsDeviceIdle() const {
  return GetDeviceIdleTime() >= idleness_cutoff_;
}

std::string DeviceCommandStartCrdSessionJob::GetRobotAccountUserName() const {
  CoreAccountId account_id = GetOAuthService().GetRobotAccountId();

  // TODO(msarda): This conversion will not be correct once account id is
  // migrated to be the Gaia ID on ChromeOS. Fix it.
  return account_id.ToString();
}

bool DeviceCommandStartCrdSessionJob::ShouldShowConfirmationDialog() const {
  switch (GetCurrentUserSessionType()) {
    case UserSessionType::AUTO_LAUNCHED_KIOSK_SESSION:
    case UserSessionType::MANUALLY_LAUNCHED_KIOSK_SESSION:
    case UserSessionType::NO_SESSION:
      return false;

    case UserSessionType::AFFILIATED_USER_SESSION:
    case UserSessionType::MANAGED_GUEST_SESSION:
    case UserSessionType::UNAFFILIATED_USER_SESSION:
    case UserSessionType::GUEST_SESSION:
      return true;

    case UserSessionType::USER_SESSION_TYPE_UNKNOWN:
      NOTREACHED();
      return true;
  }
}

bool DeviceCommandStartCrdSessionJob::ShouldTerminateUponInput() const {
  if (curtain_local_user_session_) {
    return false;
  }

  switch (GetCurrentUserSessionType()) {
    case UserSessionType::AFFILIATED_USER_SESSION:
    case UserSessionType::MANAGED_GUEST_SESSION:
      // We never terminate upon input for the user-session scenarios, because:
      //   1. There is no risk of the admin spying on the users, as they need to
      //       explicitly accept the connection request.
      //   2. If we terminate upon input the session will immediately be
      //      terminated as soon as the user accepts the connection request,
      //      as pressing the button to accept the connection request counts as
      //      user input.
      return false;

    case UserSessionType::AUTO_LAUNCHED_KIOSK_SESSION:
    case UserSessionType::MANUALLY_LAUNCHED_KIOSK_SESSION:
      return !acked_user_presence_;

    case UserSessionType::NO_SESSION:
    case UserSessionType::UNAFFILIATED_USER_SESSION:
    case UserSessionType::GUEST_SESSION:
      return true;

    case UserSessionType::USER_SESSION_TYPE_UNKNOWN:
      NOTREACHED();
      return true;
  }
}

bool DeviceCommandStartCrdSessionJob::ShouldAllowReconnections() const {
  if (!base::FeatureList::IsEnabled(
          remoting::features::kEnableCrdAdminRemoteAccessV2)) {
    return false;
  }

  // Curtained off sessions support reconnections if Chrome restarts.
  return curtain_local_user_session_;
}

bool DeviceCommandStartCrdSessionJob::ShouldShowTroubleshootingTools() const {
  return IsKioskSession(GetCurrentUserSessionType());
}

bool DeviceCommandStartCrdSessionJob::ShouldAllowTroubleshootingTools() const {
  return IsKioskSession(GetCurrentUserSessionType()) &&
         CHECK_DEREF(ProfileManager::GetActiveUserProfile()->GetPrefs())
             .GetBoolean(prefs::kKioskTroubleshootingToolsEnabled);
}

bool DeviceCommandStartCrdSessionJob::ShouldAllowFileTransfer() const {
  return IsKioskSession(GetCurrentUserSessionType()) &&
         base::FeatureList::IsEnabled(
             remoting::features::kEnableCrdFileTransferForKiosk);
}

ErrorCallback DeviceCommandStartCrdSessionJob::GetErrorCallback() {
  return base::BindOnce(&DeviceCommandStartCrdSessionJob::FinishWithError,
                        weak_factory_.GetWeakPtr());
}

void DeviceCommandStartCrdSessionJob::TerminateImpl() {
  result_callback_.Reset();
  weak_factory_.InvalidateWeakPtrs();
  delegate_->TerminateSession();
}

}  // namespace policy
