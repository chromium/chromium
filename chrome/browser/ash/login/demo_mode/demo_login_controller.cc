// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_login_controller.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/settings/user_login_permission_tracker.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {

namespace {

constexpr char kEphemeralUserDeviceIDPrefix[] = "t_";

// Demo account Json key in set up demo account response.
constexpr char kDemoAccountEmail[] = "username";
constexpr char kDemoAccountGaiaId[] = "obfuscatedGaiaId";
constexpr char kDemoAccountAuthCode[] = "authorizationCode";

constexpr char kDemoModeServerUrl[] = "https://demomode-pa.googleapis.com";
constexpr char kSetupDemoAccountEndpoint[] = "v1/accounts";
constexpr char kCleanUpDemoAccountEndpoint[] = "v1/accounts:remove";
constexpr char kApiKeyParam[] = "key";
const char kContentTypeJSON[] = "application/json";
// Request involves creating new account on server side. Setting a longer
// timeout.
constexpr base::TimeDelta kDemoAccountRequestTimeout = base::Seconds(30);

// Demo account Json key in set up demo account request:
const char kDeviceIdentifier[] = "device_identifier";
const char kDeviceMachineId[] = "serial_number";
const char kLoginScopeDeviceId[] = "login_scope_device_id";
const char kObfuscatedGaiaId[] = "obfuscated_gaia_id";
const char kDMToken[] = "dm_token";
const char kClientID[] = "client_id";

const char kDeviceInfo[] = "device_info";
const char kBuildVersion[] = "build_version";
const char kCountry[] = "country";
const char kRetailer[] = "retailer";
const char kStoreId[] = "store_id";
const char kBoard[] = "board";
const char kModel[] = "model";
const char kLocale[] = "locale";

// Maximum accepted size of an ItemSuggest response. 1MB.
constexpr int kMaxResponseSize = 1024 * 1024;

const char kErrorCodePath[] = "error.code";
const char kErrorMessagePath[] = "error.message";
const char kErrorStatusPath[] = "error.status";

constexpr base::TimeDelta kConnectPolicyManagerTimeout = base::Seconds(5);

// Server may return a 200 for setup demo account request with Quota exhuasted
// error. Sample response:
//  {
//    "status": {
//      "code": 8
//    }
//    "retryDetails": {}
//  }
constexpr char kStatusCodePath[] = "status.code";
constexpr char kRetryDetailsPath[] = "retryDetails";

// TODO(crbugs.com/355727308): Consider using
// components/enterprise/common/proto/google3_protos.proto.
constexpr int kServerResourceExhuastedCode = 8;

constexpr char kDemoModeSignInEnabledPath[] = "forceEnabled";

constexpr net::NetworkTrafficAnnotationTag kSetupAccountTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("demo_login_controller", R"(
          semantics: {
            sender: "ChromeOS Demo mode"
            description:
              "Setup demo accounts for demo mode to login regular session."
            trigger: "When login screen shown and demo mode sign in is enable."
            data: "Login scope demo accounts credential."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                email: "cros-demo-mode-eng@google.com"
              }
            }
            user_data {
              type: DEVICE_ID
            }
            last_reviewed: "2024-10-10"
          }
          policy: {
            cookies_allowed: YES
            cookies_store: "user"
            setting:
              "You could enable or disable this feature via command line flag."
              "This feature is disabled by default."
            policy_exception_justification:
              "Not implemented."
          })");

constexpr net::NetworkTrafficAnnotationTag kCleanUpTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "demo_login_controller_account_clean_up",
        R"(
          semantics: {
            sender: "ChromeOS Demo mode"
            description:
              "Clean up demo account logged in last session."
            trigger: "When login screen shown and demo mode sign in experience"
                     " is enabled."
            data: "The account id and device id to be clean up."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                email: "cros-demo-mode-eng@google.com"
              }
            }
            user_data {
              type: DEVICE_ID
              type: GAIA_ID
            }
            last_reviewed: "2024-10-30"
          }
          policy: {
            cookies_allowed: NO
            setting:
              "You could enable or disable this feature via command line flag."
              "This feature is disabled by default."
            policy_exception_justification:
              "Not implemented."
          })");

scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() {
  return g_browser_process->shared_url_loader_factory();
}

GURL GetDemoModeServerBaseUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return GURL(
      command_line->HasSwitch(switches::kDemoModeServerUrl)
          ? command_line->GetSwitchValueASCII(switches::kDemoModeServerUrl)
          : kDemoModeServerUrl);
}

GURL GetDemoAccountUrl(const std::string& endpoint) {
  GURL setup_url = GetDemoModeServerBaseUrl().Resolve(endpoint);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string api_key =
      command_line->HasSwitch(switches::kDemoModeServerAPIKey)
          ? command_line->GetSwitchValueASCII(switches::kDemoModeServerAPIKey)
          : google_apis::GetAPIKey();
  return net::AppendQueryParameter(setup_url, kApiKeyParam, api_key);
}

// TODO(crbug.com/372928818): Should use the same function in
// c/b/signin/chrome_device_id_helper.h for consistent. However there is
// circular deps issue with /c/b:browser. Temporary use this one before
// completion of modularization (crbug.com/364667553) of c/b/signin. Remove the
// prefix if device is not in ephemeral mode configured by policy.
std::string GenerateSigninScopedDeviceId() {
  return kEphemeralUserDeviceIDPrefix +
         base::Uuid::GenerateRandomV4().AsLowercaseString();
}

// ChromeOS does not allow empty password for non-ephemeral account. Generate a
// random string for cryptohome.
std::string GenerateFakePassword() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

void LoginDemoAccount(const std::string& email,
                      const GaiaId& gaia_id,
                      const std::string& auth_code,
                      const std::string& sign_in_scoped_device_id) {
  const AccountId account_id =
      AccountId::FromNonCanonicalEmail(email, gaia_id, AccountType::GOOGLE);
  // The user type is known to be regular. The unicorn flow transitions to the
  // Gaia screen and uses its own mechanism for account creation.
  std::unique_ptr<UserContext> user_context =
      login::BuildUserContextForGaiaSignIn(
          /*user_type=*/user_manager::UserType::kRegular,
          /*account_id=*/account_id,
          /*using_saml=*/false,
          /*using_saml_api=*/false,
          /*password=*/GenerateFakePassword(),
          /*password_attributes=*/SamlPasswordAttributes(),
          /*sync_trusted_vault_keys=*/std::nullopt,
          /*challenge_response_key=*/std::nullopt);
  user_context->SetAuthCode(auth_code);
  user_context->SetDeviceId(sign_in_scoped_device_id);

  // Enforced auto-login for given account creds.
  auto* login_display_host = LoginDisplayHost::default_host();
  CHECK(login_display_host);
  login_display_host->SkipPostLoginScreensForDemoMode();
  login_display_host->CompleteLogin(*user_context);
}

std::unique_ptr<network::SimpleURLLoader> CreateDemoAccountURLLoader(
    const GURL& url,
    net::NetworkTrafficAnnotationTag annotation) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          annotation);
}

// Send demo account related http requests to server. i.e. setup request,
// cleanup request.
void SendDemoAccountRequest(
    const base::Value::Dict& post_data,
    network::SimpleURLLoader* url_loader,
    base::OnceCallback<void(std::optional<std::string> response_body)>
        callback) {
  url_loader->SetAllowHttpErrorResults(true);
  url_loader->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  std::string request_string;
  CHECK(base::JSONWriter::Write(post_data, &request_string));
  url_loader->AttachStringForUpload(request_string, kContentTypeJSON);
  url_loader->SetTimeoutDuration(kDemoAccountRequestTimeout);
  url_loader->DownloadToString(GetUrlLoaderFactory().get(), std::move(callback),
                               kMaxResponseSize);
}

void LogServerResponseError(const std::string& error_response, bool is_setup) {
  if (error_response.empty()) {
    return;
  }

  std::optional<base::Value::Dict> error(base::JSONReader::ReadDict(
      error_response, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  const std::string response_name =
      base::StringPrintf("%s response error:", is_setup ? "Setup" : "Clean up");
  if (!error) {
    LOG(ERROR) << base::StringPrintf("%s Cannot parse response.",
                                     response_name);
    return;
  }
  const std::optional<int> code = error->FindIntByDottedPath(kErrorCodePath);
  const auto* msg = error->FindStringByDottedPath(kErrorMessagePath);
  const auto* status = error->FindStringByDottedPath(kErrorStatusPath);
  LOG(ERROR) << base::StringPrintf(
      "%s error code: %d; message: %s; status: %s.", response_name,
      code ? *code : -1, msg ? *msg : "", status ? *status : "");
}

DemoLoginController::ResultCode GetDemoAccountRequestResult(
    network::SimpleURLLoader* url_loader,
    base::optional_ref<std::string> response_body) {
  if (url_loader->NetError() != net::OK) {
    // TODO(crbug.com/364214790):  Handle any errors (maybe earlier for net
    // connection error) and fallback to MGS.
    return DemoLoginController::ResultCode::kNetworkError;
  }
  auto hasHeaders =
      url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers;
  int response_code = -1;
  if (hasHeaders) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }

  if (!response_body || response_body->empty()) {
    return DemoLoginController::ResultCode::kEmptyResponse;
  }

  // A request was successful if there is response body and the response code is
  // 2XX.
  bool is_success = response_code >= 200 && response_code < 300;
  return is_success ? DemoLoginController::ResultCode::kSuccess
                    : DemoLoginController::ResultCode::kRequestFailed;
}

std::string_view GetMachineID() {
  return (system::StatisticsProvider::GetInstance()->GetMachineID())
      .value_or(std::string_view());
}

void RemoveGaiaUsersOnDevice() {
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    return;
  }
  // Make a copy of the list since we'll be removing users and the list would
  // change underneath.
  const user_manager::UserList user_list = user_manager->GetPersistedUsers();
  for (const user_manager::User* user : user_list) {
    // Skip if it is ephemeral user since the user will be removed by policy.
    // Should not remove device local account.
    if (user_manager->IsEphemeralUser(user) || user->IsDeviceLocalAccount() ||
        !user->HasGaiaAccount()) {
      continue;
    }
    user_manager->RemoveUser(
        user->GetAccountId(),
        user_manager::UserRemovalReason::DEMO_ACCOUNT_CLEAN_UP);
  }
}

policy::DeviceCloudPolicyManagerAsh* GetDeviceCloudPolicyManager() {
  auto* platform_part = g_browser_process->platform_part();
  if (!platform_part) {
    LOG(ERROR) << "platform_part is null.";
    return nullptr;
  }
  auto* policy_connector_ash = platform_part->browser_policy_connector_ash();
  if (!policy_connector_ash) {
    LOG(ERROR) << "browser_policy_connector_ash is null.";
    return nullptr;
  }

  return policy_connector_ash->GetDeviceCloudPolicyManager();
}

base::Value::Dict GetDeviceInfo() {
  // Full ChromeOS version, for example: R127-15919.0.0_stable-channel.
  const std::string version = demo_mode::GetChromeOSVersionString();

  // This field "country" is intended to be used to control region specific
  // behaviors, including TOS agreement, focus backend services and etc.
  const std::string country = demo_mode::Country();

  const std::string retailer = demo_mode::RetailerName();
  const std::string store_id = demo_mode::StoreNumber();

  const std::string board = demo_mode::Board();
  const std::string_view model = demo_mode::Model();

  // This field "locale" is used to set the language of the demo account.
  const std::string locale = demo_mode::Locale();

  return base::Value::Dict()
      .Set(kBuildVersion, version)
      .Set(kCountry, country)
      .Set(kRetailer, retailer)
      .Set(kStoreId, store_id)
      .Set(kBoard, board)
      .Set(kModel, model)
      .Set(kLocale, locale);
}

}  // namespace

DemoLoginController::DemoLoginController(
    base::RepeatingClosure configure_auto_login_callback)
    : configure_auto_login_callback_(std::move(configure_auto_login_callback)) {
  state_ = State::kLoadingAvailibility;

  auto* cloud_policy_manager = GetDeviceCloudPolicyManager();
  if (!cloud_policy_manager) {
    CHECK_IS_TEST();
    state_ = State::kReadyForLoginWithDemoAccount;
    return;
  }

  is_policy_manager_connected_ = cloud_policy_manager->IsConnected();

  // Sign in experience relies on DM Token for device verification. DM Token is
  // fetched using policy client, so we need to wait for policy manager to be
  // connected.
  if (!is_policy_manager_connected_) {
    observation_.Observe(cloud_policy_manager);

    // `DemoLoginController::OnDeviceCloudPolicyManagerConnected` might not be
    // triggered if there is a network issue.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DemoLoginController::OnPolicyManagerConnectionTimeOut,
                       weak_ptr_factory_.GetWeakPtr()),
        kConnectPolicyManagerTimeout);
  }

  is_feature_eligiblity_loaded_ = features::IsDemoModeSignInEnabled();

  if (is_feature_eligiblity_loaded_ && is_policy_manager_connected_) {
    // Do not call `MaybeTriggerAutoLogin` since `DemoLoginController` is not
    // finished construction here.
    state_ = State::kReadyForLoginWithDemoAccount;
    return;
  }

  if (!is_feature_eligiblity_loaded_) {
    // If `DemoModeSignIn` is not enabled with feature flag, we fallback to
    // check if the feature is enabled by Growth Framework. This is used when we
    // are running pilot.
    LoadFeatureEligibilityFromGrowth();
  }
}

DemoLoginController::~DemoLoginController() = default;

void DemoLoginController::TriggerDemoAccountLoginFlow() {
  DCHECK_EQ(State::kReadyForLoginWithDemoAccount, state_);
  // Try demo account login first by disable auto-login to managed guest
  // session.
  state_ = State::kSetupDemoAccountInProgress;

  MaybeCleanupPreviousDemoAccount();
}

void DemoLoginController::SetSetupRequestCallbackForTesting(
    RequestCallback callback) {
  CHECK_IS_TEST();
  setup_request_callback_for_testing_ = std::move(callback);
}

void DemoLoginController::SetCleanupRequestCallbackForTesting(
    RequestCallback callback) {
  CHECK_IS_TEST();
  cleanup_request_callback_for_testing_ = std::move(callback);
}

void DemoLoginController::SetDeviceCloudPolicyManagerForTesting(
    policy::CloudPolicyManager* policy_manager) {
  policy_manager_for_testing_ = policy_manager;
}

void DemoLoginController::SendSetupDemoAccountRequest() {
  CHECK(!url_loader_);

  const auto sign_in_scoped_device_id = GenerateSigninScopedDeviceId();
  std::optional<base::Value::Dict> device_identifier =
      GetDeviceIdentifier(sign_in_scoped_device_id);
  if (!device_identifier) {
    OnSetupDemoAccountError(ResultCode::kCloudPolicyNotConnected);
    return;
  }
  // DM Token is empty.
  if (device_identifier->FindString(kDMToken)->empty()) {
    OnSetupDemoAccountError(ResultCode::kEmptyDMToken);
    return;
  }
  // Client ID is empty.
  if (device_identifier->FindString(kClientID)->empty()) {
    OnSetupDemoAccountError(ResultCode::kEmptyClientID);
    return;
  }

  auto post_data = base::Value::Dict().Set(
      kDeviceIdentifier, std::move(device_identifier.value()));

  if (features::IsSendDeviceInfoToDemoServerEnabled()) {
    base::Value::Dict device_info = GetDeviceInfo();
    post_data.Set(kDeviceInfo, std::move(device_info));
  }

  url_loader_ =
      CreateDemoAccountURLLoader(GetDemoAccountUrl(kSetupDemoAccountEndpoint),
                                 kSetupAccountTrafficAnnotation);

  SendDemoAccountRequest(
      post_data, url_loader_.get(),
      base::BindOnce(&DemoLoginController::OnSetupDemoAccountComplete,
                     weak_ptr_factory_.GetWeakPtr(), sign_in_scoped_device_id));
}

void DemoLoginController::OnSetupDemoAccountComplete(
    const std::string& sign_in_scoped_device_id,
    std::optional<std::string> response_body) {
  auto result = GetDemoAccountRequestResult(url_loader_.get(), response_body);
  url_loader_.reset();

  if (result == ResultCode::kSuccess) {
    CHECK(response_body);
    HandleSetupDemoAcountResponse(sign_in_scoped_device_id, *response_body);
  } else {
    OnSetupDemoAccountError(result);
    // `response_body` could be nullptr when network is not connected.
    if (response_body) {
      LogServerResponseError(*response_body, /*is_setup*/ true);
    }
  }
}

void DemoLoginController::HandleSetupDemoAcountResponse(
    const std::string& sign_in_scoped_device_id,
    const std::string& response_body) {
  std::optional<base::DictValue> response_json(base::JSONReader::ReadDict(
      response_body, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  if (!response_json) {
    OnSetupDemoAccountError(ResultCode::kResponseParsingError);
    return;
  }

  const std::optional<int> code =
      response_json->FindIntByDottedPath(kStatusCodePath);
  if (code && *code == kServerResourceExhuastedCode) {
    // TODO(crbugs.com/355727308): Right now, we retry with a random delay if
    // `retry_details` exists. In later version we will decide the retry delay
    // from `retry_details`.
    base::DictValue* retry_details = response_json->FindDict(kRetryDetailsPath);
    if (retry_details) {
      demo_mode::TurnOnScheduleLogoutForMGS();
      OnSetupDemoAccountError(ResultCode::kQuotaExhaustedRetriable);
    } else {
      OnSetupDemoAccountError(ResultCode::kQuotaExhaustedNotRetriable);
    }
    return;
  }

  const auto* email = response_json->FindString(kDemoAccountEmail);
  const auto* gaia_id = response_json->FindString(kDemoAccountGaiaId);
  const auto* auth_code = response_json->FindString(kDemoAccountAuthCode);
  if (!email || !gaia_id || !auth_code) {
    OnSetupDemoAccountError(ResultCode::kInvalidCreds);
    return;
  }

  // Report success to the metrics.
  DemoSessionMetricsRecorder::ReportDemoAccountSetupResult(
      ResultCode::kSuccess);

  if (setup_request_callback_for_testing_) {
    std::move(setup_request_callback_for_testing_).Run();
  }

  UserLoginPermissionTracker::Get()->SetDemoUser(
      gaia::CanonicalizeEmail(*email));
  DCHECK_EQ(State::kSetupDemoAccountInProgress, state_);
  state_ = State::kLoginDemoAccount;

  // Enable 24 hour session by overriding power policy.
  demo_mode::SetDoNothingWhenPowerIdle();
  DemoSessionMetricsRecorder::SetCurrentSessionType(
      DemoSessionMetricsRecorder::SessionType::kSignedInDemoSession);

  auto* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDemoAccountGaiaId, *gaia_id);
  local_state->SetString(prefs::kDemoModeSessionIdentifier,
                         sign_in_scoped_device_id);
  // TODO(crbug.com/383198613): Wait device local account policy loaded since we
  // applied that policy to demo account.
  LoginDemoAccount(*email, GaiaId(*gaia_id), *auth_code,
                   sign_in_scoped_device_id);
}

void DemoLoginController::OnSetupDemoAccountError(
    const ResultCode result_code) {
  // TODO(crbug.com/372333479): Instruct how to do retry on failed according to
  // the error code.

  LOG(ERROR) << "Failed to set up demo account. Result code: "
             << static_cast<int>(result_code);

  DCHECK_EQ(State::kSetupDemoAccountInProgress, state_);

  // Report error to the metrics.
  DemoSessionMetricsRecorder::ReportDemoAccountSetupResult(result_code);

  // Login public account session when set up failed.
  state_ = State::kLoginToMGS;
  DemoSessionMetricsRecorder::SetCurrentSessionType(
      DemoSessionMetricsRecorder::SessionType::kFallbackMGS);
  configure_auto_login_callback_.Run();

  if (setup_request_callback_for_testing_) {
    std::move(setup_request_callback_for_testing_).Run();
  }
}

void DemoLoginController::OnDeviceCloudPolicyManagerConnected() {
  is_policy_manager_connected_ = true;
  observation_.Reset();

  MaybeTriggerAutoLogin();
}

void DemoLoginController::OnDeviceCloudPolicyManagerGotRegistry() {
  // Do nothing.
}

void DemoLoginController::LoadFeatureEligibilityFromGrowth() {
  // Start loading growth campaign to check whether the feature is enabled.
  auto* campaigns_manager = growth::CampaignsManager::Get();
  campaigns_manager->LoadCampaigns(
      base::BindOnce(&DemoLoginController::OnCampaignsLoaded,
                     weak_ptr_factory_.GetWeakPtr()),
      /*in_oobe=*/false);
}

void DemoLoginController::MaybeCleanupPreviousDemoAccount() {
  CHECK(!url_loader_);

  // Remove gaia users on device. Usually there is only 1 gaia user from last
  // session. No-ops if device is in ephemeral mode.
  RemoveGaiaUsersOnDevice();

  // Clean up last gaia user on server side.
  auto* local_state = g_browser_process->local_state();
  const GaiaId gaia_id_to_clean_up =
      GaiaId(local_state->GetString(prefs::kDemoAccountGaiaId));
  const std::string login_scope_device_id =
      local_state->GetString(prefs::kDemoModeSessionIdentifier);
  // For the first session of demo account, `gaia_id_to_clean_up and session
  // identifier`could be empty.
  if (gaia_id_to_clean_up.empty() || login_scope_device_id.empty()) {
    SendSetupDemoAccountRequest();
    return;
  }

  auto post_data = base::Value::Dict();

  std::optional<base::Value::Dict> device_identifier =
      GetDeviceIdentifier(login_scope_device_id);
  if (!device_identifier) {
    OnCleanUpDemoAccountError(ResultCode::kCloudPolicyNotConnected);
    // Try requesting for a new demo account regardless of the cleanup result.
    SendSetupDemoAccountRequest();
    return;
  }
  // DM Token is empty.
  if (device_identifier->FindString(kDMToken)->empty()) {
    OnCleanUpDemoAccountError(ResultCode::kEmptyDMToken);
    // Try requesting for a new demo account regardless of the cleanup result.
    SendSetupDemoAccountRequest();
    return;
  }
  // Client ID is empty.
  if (device_identifier->FindString(kClientID)->empty()) {
    OnCleanUpDemoAccountError(ResultCode::kEmptyClientID);
    // Try requesting for a new demo account regardless of the cleanup result.
    SendSetupDemoAccountRequest();
    return;
  }

  post_data.Set(kDeviceIdentifier, std::move(device_identifier.value()));
  post_data.Set(kObfuscatedGaiaId, gaia_id_to_clean_up.ToString());

  url_loader_ =
      CreateDemoAccountURLLoader(GetDemoAccountUrl(kCleanUpDemoAccountEndpoint),
                                 kCleanUpTrafficAnnotation);

  SendDemoAccountRequest(
      post_data, url_loader_.get(),
      base::BindOnce(&DemoLoginController::OnCleanUpDemoAccountComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoLoginController::OnCleanUpDemoAccountComplete(
    std::optional<std::string> response_body) {
  auto result = GetDemoAccountRequestResult(url_loader_.get(), response_body);

  if (result == ResultCode::kSuccess) {
    // Report success to the metrics.
    DemoSessionMetricsRecorder::ReportDemoAccountCleanupResult(result);

    // Clear the the gaia_id and sign_in_scoped_device_id in pref to prevent
    // repeating cleanups.
    auto* local_state = g_browser_process->local_state();
    local_state->ClearPref(prefs::kDemoAccountGaiaId);
    local_state->ClearPref(prefs::kDemoModeSessionIdentifier);

    if (cleanup_request_callback_for_testing_) {
      std::move(cleanup_request_callback_for_testing_).Run();
    }
  } else {
    // `response_body` could be nullptr when network is not connected.
    if (response_body) {
      LogServerResponseError(*response_body, /*is_setup*/ false);
    }
    OnCleanUpDemoAccountError(result);
  }
  url_loader_.reset();
  // Try request for new demo account regardless of the cleanup result.
  SendSetupDemoAccountRequest();
}

void DemoLoginController::OnCleanUpDemoAccountError(
    const ResultCode result_code) {
  // Report error to the metrics.
  DemoSessionMetricsRecorder::ReportDemoAccountCleanupResult(result_code);

  LOG(ERROR) << "Failed to clean up demo account. Result code: "
             << static_cast<int>(result_code);

  if (cleanup_request_callback_for_testing_) {
    std::move(cleanup_request_callback_for_testing_).Run();
  }
}

std::optional<base::Value::Dict> DemoLoginController::GetDeviceIdentifier(
    const std::string& login_scope_device_id) {
  // The class member `policy_manager_for_testing_` is set during testing.
  // If it's not set, it means we're not in the testing environment, so we
  // can get the real policy manager from `policy_connector_ash`.
  policy::CloudPolicyManager* policy_manager =
      policy_manager_for_testing_ ? policy_manager_for_testing_
                                  : GetDeviceCloudPolicyManager();

  if (!policy_manager) {
    LOG(ERROR)
        << "device_cloud_policy_manager is null, or it's not set for testing.";
    return std::nullopt;
  }
  auto* core = policy_manager->core();
  if (!core) {
    LOG(ERROR) << "Cloud_policy_core is null.";
    return std::nullopt;
  }
  policy::CloudPolicyClient* client = core->client();
  if (!client) {
    LOG(ERROR) << "cloud_policy_client is null.";
    return std::nullopt;
  }
  std::string dm_token = client->dm_token();
  std::string client_id = client->client_id();
  return base::Value::Dict()
      .Set(kDMToken, dm_token)
      .Set(kClientID, client_id)
      .Set(kDeviceMachineId, GetMachineID())
      .Set(kLoginScopeDeviceId, login_scope_device_id);
}

void DemoLoginController::HandleFeatureEligibility(bool is_sign_in_enable) {
  // Enable sign in demo account globally:
  demo_mode::SetForceEnableDemoAccountSignIn(is_sign_in_enable);

  is_feature_eligiblity_loaded_ = true;
  MaybeTriggerAutoLogin();
}

void DemoLoginController::OnCampaignsLoaded() {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  auto* campaign = campaigns_manager->GetCampaignBySlot(
      growth::Slot::kDemoModeSignInExperience);

  // TODO(crbug.com/364214790): Record metric for the campaigns loading failure.
  if (!campaign) {
    VLOG(1) << "No campaign matched. Fallback to login as MGS.";
    HandleFeatureEligibility(/*is_sign_in_enable=*/false);
    return;
  }

  auto* payload =
      GetPayloadBySlot(campaign, growth::Slot::kDemoModeSignInExperience);
  if (!payload) {
    VLOG(1) << "No valid payload found.Fallback to login as MGS.";
    HandleFeatureEligibility(/*is_sign_in_enable=*/false);
    return;
  }
  std::optional<bool> enabled = payload->FindBool(kDemoModeSignInEnabledPath);

  bool force_demo_sign_in_by_growth = enabled.has_value() && *enabled;
  HandleFeatureEligibility(/*is_sign_in_enable=*/force_demo_sign_in_by_growth);
}

void DemoLoginController::MaybeTriggerAutoLogin() {
  CHECK_EQ(State::kLoadingAvailibility, state_);

  bool is_policy_manager_loading_finished =
      is_policy_manager_connected_ || is_loading_policy_manager_timeout_;
  bool is_loading_finished =
      is_policy_manager_loading_finished && is_feature_eligiblity_loaded_;

  if (!is_loading_finished) {
    return;
  }

  bool is_sign_in_enable = demo_mode::IsDemoAccountSignInEnabled();
  state_ = is_sign_in_enable && is_policy_manager_connected_
               ? State::kReadyForLoginWithDemoAccount
               : State::kLoginToMGS;

  configure_auto_login_callback_.Run();
}

void DemoLoginController::OnPolicyManagerConnectionTimeOut() {
  if (is_policy_manager_connected_) {
    return;
  }

  is_loading_policy_manager_timeout_ = true;
  observation_.Reset();

  DemoSessionMetricsRecorder::RecordCloudPolicyConnectionTimeout();
  SYSLOG(INFO) << "Timeout for waiting cloud policy manager connected. Login "
                  "to managed guest session.";
  MaybeTriggerAutoLogin();
}

}  // namespace ash
