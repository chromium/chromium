// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_login_controller.h"

#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"
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
// Attestation based device identifier.
const char kDeviceADID[] = "cros_adid";
const char kLoginScopeDeviceId[] = "login_scope_device_id";
const char kObfuscatedGaiaId[] = "obfuscated_gaia_id";

// Maximum accepted size of an ItemSuggest response. 1MB.
constexpr int kMaxResponseSize = 1024 * 1024;

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
// completion of modularization (crbug.com/364667553) of c/b/signin.
std::string GenerateSigninScopedDeviceId() {
  return kEphemeralUserDeviceIDPrefix +
         base::Uuid::GenerateRandomV4().AsLowercaseString();
}

void LoginDemoAccount(const std::string& email,
                      const std::string& gaia_id,
                      const std::string& auth_code,
                      const std::string& sign_in_scoped_device_id) {
  // TODO(crbug.com/364195755): Allow list this user in CrosSetting when the
  // request is success.
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
          /*password=*/"",
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
    base::OnceCallback<void(std::unique_ptr<std::string> response_body)>
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

DemoLoginController::ResultCode GetDemoAccountRequestResult(
    network::SimpleURLLoader* url_loader,
    const std::string& response_body) {
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

  if (response_body.empty()) {
    return DemoLoginController::ResultCode::kEmptyReponse;
  }

  // A request was successful if there is response body and the response code is
  // 2XX.
  bool is_success = response_code >= 200 && response_code < 300;
  return is_success ? DemoLoginController::ResultCode::kSuccess
                    : DemoLoginController::ResultCode::kRequestFailed;
}

void OnCleanUpDemoAccountError(
    const DemoLoginController::ResultCode result_code) {
  // TODO(crbug.com/364214790): Record metric for the failure.
  LOG(ERROR) << "Failed to clean up demo account. Result code: "
             << static_cast<int>(result_code);
}

std::string GetDeviceADID() {
  // TODO(crbug.com/372762477): Get device adid form enterprise. Temporary set
  // as "0000" right now.
  return "0000";
}

base::Value::Dict GetDeviceIdentifier(
    const std::string& login_scope_device_id) {
  return base::Value::Dict()
      .Set(kDeviceADID, GetDeviceADID())
      .Set(kLoginScopeDeviceId, login_scope_device_id);
}

}  // namespace

DemoLoginController::DemoLoginController(
    LoginScreenClientImpl* login_screen_client) {
  CHECK(login_screen_client);
  scoped_observation_.Observe(login_screen_client);
}

DemoLoginController::~DemoLoginController() = default;

void DemoLoginController::OnLoginScreenShown() {
  // Stop observe login screen since it may get invoked in session. Demo account
  // should be setup only once for each session. Follow up response will
  // instruct retry or fallback to public account.
  scoped_observation_.Reset();

  if (!demo_mode::IsDeviceInDemoMode()) {
    return;
  }

  // TODO(crbug.com/370806573): Skip auto login public account in
  // `ExistingUserController::StartAutoLoginTimer` if this feature enable
  // Maybe add a policy.
  MaybeCleanupPreviousDemoAccount();
}

void DemoLoginController::SetSetupFailedCallbackForTest(
    base::OnceCallback<void(const ResultCode result_code)> callback) {
  setup_failed_callback_for_testing_ = std::move(callback);
}

void DemoLoginController::SetCleanUpFailedCallbackForTest(
    base::OnceCallback<void(const ResultCode result_code)> callback) {
  clean_up_failed_callback_for_testing_ = std::move(callback);
}

void DemoLoginController::SendSetupDemoAccountRequest() {
  CHECK(!url_loader_);

  // TODO(crbug.com/372333479): Demo server use auth the request with device
  // integrity check. Attach credential to the request once it is ready.
  const auto sign_in_scoped_device_id = GenerateSigninScopedDeviceId();
  auto post_data = base::Value::Dict().Set(
      kDeviceIdentifier, GetDeviceIdentifier(sign_in_scoped_device_id));
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
    std::unique_ptr<std::string> response_body) {
  auto result = GetDemoAccountRequestResult(url_loader_.get(), *response_body);
  url_loader_.reset();
  if (result == ResultCode::kSuccess) {
    HandleSetupDemoAcountResponse(sign_in_scoped_device_id,
                                  std::move(response_body));
  } else {
    // TODO(crbug.com/364214790):  Handle any errors (maybe earlier for net
    // connection error) and fallback to MGS.
    OnSetupDemoAccountError(result);
  }
}

void DemoLoginController::HandleSetupDemoAcountResponse(
    const std::string& sign_in_scoped_device_id,
    const std::unique_ptr<std::string> response_body) {
  std::optional<base::Value::Dict> gaia_creds(
      base::JSONReader::ReadDict(*response_body));
  if (!gaia_creds) {
    OnSetupDemoAccountError(ResultCode::kResponseParsingError);
    return;
  }

  const auto* email = gaia_creds->FindString(kDemoAccountEmail);
  const auto* gaia_id = gaia_creds->FindString(kDemoAccountGaiaId);
  const auto* auth_code = gaia_creds->FindString(kDemoAccountAuthCode);
  if (!email || !gaia_id || !auth_code) {
    OnSetupDemoAccountError(ResultCode::kInvalidCreds);
    return;
  }

  auto* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDemoAccountGaiaId, *gaia_id);

  LoginDemoAccount(*email, *gaia_id, *auth_code, sign_in_scoped_device_id);
}

void DemoLoginController::OnSetupDemoAccountError(
    const DemoLoginController::ResultCode result_code) {
  // TODO(crbug.com/372333479): Instruct how to do retry on failed according to
  // the error code.
  LOG(ERROR) << "Failed to set up demo account. Result code: "
             << static_cast<int>(result_code);
  if (setup_failed_callback_for_testing_) {
    std::move(setup_failed_callback_for_testing_).Run(result_code);
  }
}

void DemoLoginController::MaybeCleanupPreviousDemoAccount() {
  CHECK(!url_loader_);

  const std::string gaia_id_to_clean_up =
      g_browser_process->local_state()->GetString(prefs::kDemoAccountGaiaId);
  // For the first session of demo account, `gaia_id_to_clean_up` could be
  // empty.
  if (gaia_id_to_clean_up.empty()) {
    SendSetupDemoAccountRequest();
    return;
  }

  auto post_data = base::Value::Dict();
  // TODO(crbug.com/370808139): Get last login scope device id in locale state
  // use "0000" for now.
  post_data.Set(kDeviceIdentifier,
                GetDeviceIdentifier(/*login_scope_device_id=*/"0000"));
  post_data.Set(kObfuscatedGaiaId, gaia_id_to_clean_up);

  url_loader_ =
      CreateDemoAccountURLLoader(GetDemoAccountUrl(kCleanUpDemoAccountEndpoint),
                                 kCleanUpTrafficAnnotation);

  SendDemoAccountRequest(
      post_data, url_loader_.get(),
      base::BindOnce(&DemoLoginController::OnCleanUpDemoAccountComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoLoginController::OnCleanUpDemoAccountComplete(
    std::unique_ptr<std::string> response_body) {
  auto result = GetDemoAccountRequestResult(url_loader_.get(), *response_body);
  if (result != ResultCode::kSuccess) {
    if (clean_up_failed_callback_for_testing_) {
      std::move(clean_up_failed_callback_for_testing_).Run(result);
    } else {
      OnCleanUpDemoAccountError(result);
    }
  }

  url_loader_.reset();
  // Try request for new demo account regardless clean up result.
  SendSetupDemoAccountRequest();
}

}  // namespace ash
