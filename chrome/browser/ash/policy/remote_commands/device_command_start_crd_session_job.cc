// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user_manager.h"
#include "extensions/common/value_builder.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "remoting/host/chromeos/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

namespace mojom = chromeos::network_config::mojom;

using ResultCode = DeviceCommandStartCrdSessionJob::ResultCode;
using extensions::DictionaryBuilder;

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

// Helper method that DVLOGs all the given networks.
void LogNetworks(
    std::vector<mojom::NetworkStatePropertiesPtr>::const_iterator begin,
    std::vector<mojom::NetworkStatePropertiesPtr>::const_iterator end,
    const char* type) {
  CRD_DVLOG(3) << "Found " << (end - begin) << " " << type << " networks:";
  for (auto it = begin; it < end; it++) {
    const mojom::NetworkStateProperties& network = *it->get();
    CRD_DVLOG(3) << "   --> " << network.name << " (" << network.guid << "): "
                 << " ONC source: " << network.source
                 << ", Type: " << network.type;
  }
}

ash::KioskAppManagerBase* GetKioskAppManagerIfKioskAppIsRunning(
    const user_manager::UserManager* user_manager) {
  if (user_manager->IsLoggedInAsKioskApp())
    return ash::KioskAppManager::Get();
  if (user_manager->IsLoggedInAsArcKioskApp())
    return ash::ArcKioskAppManager::Get();
  if (user_manager->IsLoggedInAsWebKioskApp())
    return ash::WebKioskAppManager::Get();

  return nullptr;
}

void SendResultCodeToUma(ResultCode result_code) {
  base::UmaHistogramEnumeration("Enterprise.DeviceRemoteCommand.Crd.Result",
                                result_code);
}

void SendSessionTypeToUma(
    DeviceCommandStartCrdSessionJob::UmaSessionType session_type) {
  base::UmaHistogramEnumeration(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType", session_type);
}

bool IsNetworkManagedByPolicy(
    const mojom::NetworkStatePropertiesPtr& network_properties) {
  return network_properties->source == mojom::OncSource::kDevicePolicy ||
         network_properties->source == mojom::OncSource::kUserPolicy;
}

// Returns if the ChromeOS device is in a managed environment or not.
// We consider our environment to be managed if there is a
//      * active (connected) network
//      * with an ONC source set (meaning the network is managed)
//      * which is not cellular
//
// The reasoning is that these conditions will only be met if the device is in
// an office building or store, and these conditions will not be met if the
// device is in a private setting like an user's home.
bool IsInManagedEnvironment(
    std::vector<mojom::NetworkStatePropertiesPtr> active_networks) {
  const auto begin = active_networks.begin();
  auto end = active_networks.end();

  LogNetworks(begin, end, "active");

  // Filter out the unmanaged networks.
  end = std::remove_if(begin, end, [](const auto& network) {
    return !IsNetworkManagedByPolicy(network);
  });

  // Filter out cellular networks, as managed cellular networks might be found
  // even at the user's home.
  end = std::remove_if(begin, end, [](const auto& network) {
    return network->type == mojom::NetworkType::kCellular;
  });

  LogNetworks(begin, end, "managed");

  // Now if any networks remain we are in a managed environment.
  bool is_empty = (begin == end);
  return !is_empty;
}

std::string CreateSuccessPayload(const std::string& access_code) {
  return DictionaryBuilder()
      .Set(kResultCodeFieldName, static_cast<int>(ResultCode::SUCCESS))
      .Set(kResultAccessCodeFieldName, access_code)
      .ToJSON();
}

std::string CreateNonIdlePayload(const base::TimeDelta& time_delta) {
  return DictionaryBuilder()
      .Set(kResultCodeFieldName, static_cast<int>(ResultCode::FAILURE_NOT_IDLE))
      .Set(kResultLastActivityFieldName,
           static_cast<int>(time_delta.InSeconds()))
      .ToJSON();
}

std::string CreateErrorPayload(ResultCode result_code,
                               const std::string& error_message) {
  DCHECK(result_code != ResultCode::SUCCESS);
  DCHECK(result_code != ResultCode::FAILURE_NOT_IDLE);

  DictionaryBuilder builder;
  builder.Set(kResultCodeFieldName, static_cast<int>(result_code));
  if (!error_message.empty()) {
    builder.Set(kResultMessageFieldName, error_message);
  }
  return builder.ToJSON();
}

DeviceOAuth2TokenService& GetOAuthService() {
  return CHECK_DEREF(DeviceOAuth2TokenServiceFactory::Get());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ManagedNetworkChecker
////////////////////////////////////////////////////////////////////////////////

// Helper class that asynchronously fetches the list of active (connected)
// networks, and uses that information to decide if the admin is allowed to
// start a curtained session.
//
// See `IsInManagedEnvironment()` for a more detailed description on the
// algorithm used.
class DeviceCommandStartCrdSessionJob::ManagedNetworkChecker {
 public:
  using SuccessCallback = base::OnceClosure;

  ManagedNetworkChecker(SuccessCallback success_callback,
                        ErrorCallback error_callback)
      : success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)) {}
  ManagedNetworkChecker(const ManagedNetworkChecker&) = delete;
  ManagedNetworkChecker& operator=(const ManagedNetworkChecker&) = delete;
  ~ManagedNetworkChecker() = default;

  void Start() {
    BindMojomService();
    GetActiveNetworksAsync(
        base::BindOnce(&ManagedNetworkChecker::CheckIsInManagedEnvironment,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void BindMojomService() {
    ash::network_config::BindToInProcessInstance(
        network_service_.BindNewPipeAndPassReceiver());
  }

  void GetActiveNetworksAsync(
      mojom::CrosNetworkConfig::GetNetworkStateListCallback on_success) {
    CRD_DVLOG(1) << "Fetching active networks";
    network_service_->GetNetworkStateList(
        mojom::NetworkFilter::New(mojom::FilterType::kActive,  //
                                  mojom::NetworkType::kAll,    //
                                  mojom::kNoLimit),
        std::move(on_success));
  }

  void CheckIsInManagedEnvironment(
      std::vector<mojom::NetworkStatePropertiesPtr> networks) {
    CRD_DVLOG(1) << "Received active networks";

    if (IsInManagedEnvironment(std::move(networks))) {
      std::move(success_callback_).Run();
    } else {
      std::move(error_callback_)
          .Run(ResultCode::FAILURE_UNMANAGED_ENVIRONMENT, "");
    }
  }

  mojo::Remote<mojom::CrosNetworkConfig> network_service_;

  SuccessCallback success_callback_;
  ErrorCallback error_callback_;

  base::WeakPtrFactory<ManagedNetworkChecker> weak_factory_{this};
};

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
    oauth_request_ = oauth_service_.StartAccessTokenRequest(scopes, this);
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

  DeviceOAuth2TokenService& oauth_service_;
  absl::optional<std::string> oauth_token_for_test_;
  DeviceCommandStartCrdSessionJob::OAuthTokenCallback success_callback_;
  DeviceCommandStartCrdSessionJob::ErrorCallback error_callback_;
  // Handler for the OAuth access token request.
  // When deleted the token manager will cancel the request (and not call us).
  std::unique_ptr<OAuth2AccessTokenManager::Request> oauth_request_;
};

////////////////////////////////////////////////////////////////////////////////
// DeviceCommandStartCrdSessionJob
////////////////////////////////////////////////////////////////////////////////

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
  DCHECK_EQ(managed_network_checker_, nullptr);

  if (!curtain_local_user_session_) {
    // No need to check for managed networks if we are not going to curtain
    // off the local session.
    std::move(on_success).Run();
    return;
  }

  managed_network_checker_ = std::make_unique<ManagedNetworkChecker>(
      /*on_success=*/std::move(on_success),
      /*on_error=*/GetErrorCallback());

  managed_network_checker_->Start();
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(succeeded_callback_),
                                CreateSuccessPayload(access_code)));
}

void DeviceCommandStartCrdSessionJob::FinishWithError(
    const ResultCode result_code,
    const std::string& message) {
  DCHECK(result_code != ResultCode::SUCCESS);
  CRD_LOG(INFO) << "Not starting CRD session because of error (code "
                << static_cast<int>(result_code) << ", message '" << message
                << "')";
  if (!failed_callback_)
    return;  // Task was terminated.

  SendResultCodeToUma(result_code);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(failed_callback_),
                                CreateErrorPayload(result_code, message)));
}

void DeviceCommandStartCrdSessionJob::FinishWithNotIdleError() {
  CRD_LOG(INFO) << "Not starting CRD session because device is not idle";
  if (!failed_callback_)
    return;  // Task was terminated.

  SendResultCodeToUma(ResultCode::FAILURE_NOT_IDLE);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(failed_callback_),
                     CreateNonIdlePayload(GetDeviceIdlenessPeriod())));
}

bool DeviceCommandStartCrdSessionJob::UserTypeSupportsCrd() const {
  const UserSessionType current_user_type = GetUserSessionType();

  CRD_DVLOG(2) << "User is of type " << UserTypeToString(current_user_type);

  if (curtain_local_user_session_) {
    return current_user_type == UserSessionType::kNoUser;
  }

  switch (current_user_type) {
    case UserSessionType::kAffiliatedUser:
    case UserSessionType::kAutoLaunchedKiosk:
    case UserSessionType::kManagedGuestSession:
    case UserSessionType::kManuallyLaunchedKiosk:
      return true;
    case UserSessionType::kNoUser:
    case UserSessionType::kOther:
      return false;
  }
  NOTREACHED();
  return false;
}

DeviceCommandStartCrdSessionJob::UserSessionType
DeviceCommandStartCrdSessionJob::GetUserSessionType() const {
  const auto* user_manager = user_manager::UserManager::Get();

  if (!user_manager->IsUserLoggedIn())
    return UserSessionType::kNoUser;

  if (user_manager->IsLoggedInAsAnyKioskApp()) {
    if (IsRunningAutoLaunchedKiosk())
      return UserSessionType::kAutoLaunchedKiosk;
    else
      return UserSessionType::kManuallyLaunchedKiosk;
  }

  if (user_manager->IsLoggedInAsPublicAccount())
    return UserSessionType::kManagedGuestSession;

  if (user_manager->GetActiveUser()->IsAffiliated())
    return UserSessionType::kAffiliatedUser;

  return UserSessionType::kOther;
}

DeviceCommandStartCrdSessionJob::UmaSessionType
DeviceCommandStartCrdSessionJob::GetUmaSessionType() const {
  switch (GetUserSessionType()) {
    case UserSessionType::kAutoLaunchedKiosk:
      return UmaSessionType::kAutoLaunchedKiosk;
    case UserSessionType::kAffiliatedUser:
      return UmaSessionType::kAffiliatedUser;
    case UserSessionType::kManagedGuestSession:
      return UmaSessionType::kManagedGuestSession;
    case UserSessionType::kManuallyLaunchedKiosk:
      return UmaSessionType::kManuallyLaunchedKiosk;
    case UserSessionType::kNoUser:
      // TODO(b/236689277): Introduce UmaSessionType::kNoLocalUser.
      return UmaSessionType::kMaxValue;
    case UserSessionType::kOther:
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
  CoreAccountId account_id = GetOAuthService().GetRobotAccountId();

  // TODO(msarda): This conversion will not be correct once account id is
  // migrated to be the Gaia ID on ChromeOS. Fix it.
  return account_id.ToString();
}

bool DeviceCommandStartCrdSessionJob::ShouldShowConfirmationDialog() const {
  switch (GetUserSessionType()) {
    case UserSessionType::kAffiliatedUser:
    case UserSessionType::kManagedGuestSession:
      return true;
    case UserSessionType::kAutoLaunchedKiosk:
    case UserSessionType::kManuallyLaunchedKiosk:
    case UserSessionType::kNoUser:
    case UserSessionType::kOther:
      return false;
  }
  NOTREACHED();
  return false;
}

bool DeviceCommandStartCrdSessionJob::ShouldTerminateUponInput() const {
  if (curtain_local_user_session_)
    return false;

  switch (GetUserSessionType()) {
    case UserSessionType::kAffiliatedUser:
    case UserSessionType::kManagedGuestSession:
      // We never terminate upon input for the user-session scenarios, because:
      //   1. There is no risk of the admin spying on the users, as they need to
      //       explicitly accept the connection request.
      //   2. If we terminate upon input the session will immediately be
      //      terminated as soon as the user accepts the connection request,
      //      as pressing the button to accept the connection request counts as
      //      user input.
      return false;
    case UserSessionType::kAutoLaunchedKiosk:
    case UserSessionType::kManuallyLaunchedKiosk:
      return !acked_user_presence_;
    case UserSessionType::kNoUser:
    case UserSessionType::kOther:
      // This method will only be called for user types for which we support
      // CRD sessions.
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

DeviceCommandStartCrdSessionJob::ErrorCallback
DeviceCommandStartCrdSessionJob::GetErrorCallback() {
  return base::BindOnce(&DeviceCommandStartCrdSessionJob::FinishWithError,
                        weak_factory_.GetWeakPtr());
}

void DeviceCommandStartCrdSessionJob::TerminateImpl() {
  succeeded_callback_.Reset();
  failed_callback_.Reset();
  weak_factory_.InvalidateWeakPtrs();
  delegate_->TerminateSession(base::OnceClosure());
}

const char* DeviceCommandStartCrdSessionJob::UserTypeToString(
    UserSessionType value) const {
  switch (value) {
    case UserSessionType::kAutoLaunchedKiosk:
      return "kAutoLaunchedKiosk";
    case UserSessionType::kManuallyLaunchedKiosk:
      return "kManuallyLaunchedKiosk";
    case UserSessionType::kNoUser:
      return "kNoUser";
    case UserSessionType::kAffiliatedUser:
      return "kAffiliatedUser";
    case UserSessionType::kManagedGuestSession:
      return "kManagedGuestSession";
    case UserSessionType::kOther:
      return "kOther";
  }
  NOTREACHED();
  return "<invalid user type>";
}

}  // namespace policy
