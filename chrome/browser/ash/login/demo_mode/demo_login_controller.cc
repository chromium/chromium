// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_login_controller.h"

#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"

namespace ash {

namespace {

// Demo account Json key in set up demo account response.
constexpr char kDemoAccountEmail[] = "username";
constexpr char kDemoAccountGaiaId[] = "gaiaId";
constexpr char kDemoAccountAuthCode[] = "authorizationCode";

void LoginDemoAccount(const std::string& email,
                      const std::string& gaia_id,
                      const std::string& auth_code) {
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

  // Enforced auto-login for given account creds.
  auto* login_display_host = LoginDisplayHost::default_host();
  CHECK(login_display_host);
  // TODO(crbug.com/364214790): Login scoped device id for ephemeral account is
  // generated after demo account creation. Get it before calling
  // `CompleteLogin`.
  login_display_host->CompleteLogin(*user_context);
}

// TODO(crbug.com/364214790): Handle Setup demo account errors.
void OnSetupError(const DemoLoginController::ResultCode result_code) {
  LOG(ERROR) << "Failed to set up demo account. Result code: "
             << static_cast<int>(result_code);
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

void DemoLoginController::SetSetupDemoAccountResponseForTest(
    const std::string& setup_demo_account_response) {
  setup_demo_account_response_ = setup_demo_account_response;
}

void DemoLoginController::SendSetupDemoAccountRequest() {
  // TODO(crbug.com/364214790):
  // Implement simple url loader to send http request for setup demo account.
  // Call `OnSetupDemoAccountComplete` on request finished. If you are testing
  // with OTA, simply inject its Gaia creds to `setup_demo_account_response_`.
  OnSetupDemoAccountComplete();
}

void DemoLoginController::OnSetupDemoAccountComplete() {
  // TODO(crbug.com/364214790): Handle response error and decides whether
  // fallback to MGS or conduct a retry.
  if (setup_demo_account_response_.empty()) {
    OnSetupError(ResultCode::kEmptyReponse);
    return;
  }
  ParseSetupDemoAccountResponse(setup_demo_account_response_);
}

void DemoLoginController::ParseSetupDemoAccountResponse(
    const std::string& response_body) {
  std::optional<base::Value::Dict> gaia_creds(
      base::JSONReader::ReadDict(response_body));

  if (!gaia_creds) {
    OnSetupError(ResultCode::kResponseParsingError);
    return;
  }

  const auto* email = gaia_creds->FindString(kDemoAccountEmail);
  const auto* gaia_id = gaia_creds->FindString(kDemoAccountGaiaId);
  const auto* auth_code = gaia_creds->FindString(kDemoAccountAuthCode);
  if (!email || !gaia_id || !auth_code) {
    OnSetupError(ResultCode::kInvalidCreds);
    return;
  }

  LoginDemoAccount(*email, *gaia_id, *auth_code);
}

// TODO(crbug.com/370808139): Implement account clean up on session end.
// Persist its state to locale state if not success and try again on login
// screen.

}  // namespace ash
