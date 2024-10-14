// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_login_controller.h"

#include <optional>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
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

// Maximum accepted size of an ItemSuggest response. 1MB.
constexpr int kMaxResponseSize = 1024 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
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
              "This feature is diabled by default."
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

GURL GetSetupDemoAccountUrl() {
  GURL setup_url =
      GetDemoModeServerBaseUrl().Resolve(kSetupDemoAccountEndpoint);
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
                      const std::string& device_id) {
  // TODO(crbug.com/364195755): Allow list this user in CrosSetting when the
  // request is success.
  // TODO(crbug.com/364195323):After login with a demo account, several screens
  // (e.g. Chrome sync consent/personalization...)appears. Skips these screen.
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
  user_context->SetDeviceId(device_id);

  // Enforced auto-login for given account creds.
  auto* login_display_host = LoginDisplayHost::default_host();
  CHECK(login_display_host);
  // TODO(crbug.com/364214790): Login scoped device id for ephemeral account is
  // generated after demo account creation. Get it before calling
  // `CompleteLogin`.
  login_display_host->CompleteLogin(*user_context);
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

  // TODO(crbug.com/370806573): Implement account clean for backup on login
  // screen in case the it fail on shutdown.

  // TODO(crbug.com/370806573): Skip auto login public account in
  // `ExistingUserController::StartAutoLoginTimer` if this feature enable
  // Maybe add a policy.
  SendSetupDemoAccountRequest();
}

void DemoLoginController::SetSetupFailedCallbackForTest(
    base::OnceCallback<void(const ResultCode result_code)> callback) {
  setup_failed_callback_for_testing_ = std::move(callback);
}

void DemoLoginController::SendSetupDemoAccountRequest() {
  // We should not start a second request before current setup request finish.
  if (setup_request_url_loader_) {
    return;
  }

  // TODO(crbug.com/372333479): Demo server use auth the request with device
  // integrity check. Attach credential to the request once it is ready.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(GetSetupDemoAccountUrl());
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  setup_request_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  setup_request_url_loader_->SetAllowHttpErrorResults(true);
  setup_request_url_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  auto post_data = base::Value::Dict();
  // TODO(crbug.com/372762477): Get device adid form enterprise. Temporary set
  // as "0000" right now.
  const std::string device_id = GenerateSigninScopedDeviceId();
  post_data.Set(kDeviceIdentifier, base::Value::Dict()
                                       .Set(kDeviceADID, "0000")
                                       .Set(kLoginScopeDeviceId, device_id));
  std::string request_string;
  CHECK(base::JSONWriter::Write(post_data, &request_string));
  setup_request_url_loader_->AttachStringForUpload(request_string,
                                                   kContentTypeJSON);
  setup_request_url_loader_->SetTimeoutDuration(kDemoAccountRequestTimeout);
  setup_request_url_loader_->DownloadToString(
      GetUrlLoaderFactory().get(),
      base::BindOnce(&DemoLoginController::OnSetupDemoAccountComplete,
                     weak_ptr_factory_.GetWeakPtr(), device_id),
      kMaxResponseSize);
}

void DemoLoginController::OnSetupDemoAccountComplete(
    const std::string& device_id,
    std::unique_ptr<std::string> response_body) {
  if (setup_request_url_loader_->NetError() != net::OK) {
    // TODO(crbug.com/364214790):  Handle any errors (maybe earlier for net
    // connection error) and fallback to MGS.
    setup_request_url_loader_.reset();
    OnSetupDemoAccountError(ResultCode::kNetworkError);
    return;
  }

  auto hasHeaders = setup_request_url_loader_->ResponseInfo() &&
                    setup_request_url_loader_->ResponseInfo()->headers;
  int response_code = -1;
  if (hasHeaders) {
    response_code =
        setup_request_url_loader_->ResponseInfo()->headers->response_code();
  }

  // A request was successful if there is response body and the response code is
  // 2XX.
  bool is_success =
      response_body && response_code >= 200 && response_code < 300;
  if (is_success) {
    HandleSetupDemoAcountResponse(device_id, *response_body);
  } else if (!response_body) {
    OnSetupDemoAccountError(ResultCode::kEmptyReponse);
  } else {
    // TODO(crbug.com/372333479): Instruct how to do retry on failed.
    OnSetupDemoAccountError(ResultCode::kRequestFailed);
  }
  setup_request_url_loader_.reset();
}

void DemoLoginController::HandleSetupDemoAcountResponse(
    const std::string& device_id,
    const std::string& response_body) {
  std::optional<base::Value::Dict> gaia_creds(
      base::JSONReader::ReadDict(response_body));

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

  LoginDemoAccount(*email, *gaia_id, *auth_code, device_id);
}

// TODO(crbug.com/364214790): Handle Setup demo account errors.
void DemoLoginController::OnSetupDemoAccountError(
    const DemoLoginController::ResultCode result_code) {
  LOG(ERROR) << "Failed to set up demo account. Result code: "
             << static_cast<int>(result_code);
  if (setup_failed_callback_for_testing_) {
    std::move(setup_failed_callback_for_testing_).Run(result_code);
  }
}

// TODO(crbug.com/370808139): Implement account clean up on session end.
// Persist its state to locale state if not success and try again on login
// screen.

}  // namespace ash
