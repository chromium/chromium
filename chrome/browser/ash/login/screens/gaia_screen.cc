// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gaia_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chromeos/ash/components/login/auth/public/sync_trusted_vault_keys.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/base/features.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash {
namespace {

constexpr char kUserActionBack[] = "back";
constexpr char kUserActionCancel[] = "cancel";
constexpr char kUserActionStartEnrollment[] = "startEnrollment";
constexpr char kUserActionReloadDefault[] = "reloadDefault";
constexpr char kUserActionRetry[] = "retry";
constexpr char kUserActionCompleteAuth[] = "completeAuthentication";
constexpr char kUserActionCompleteLoginForTesting[] = "completeLoginForTesting";
constexpr char kUserActionEnterIdentifier[] = "identifierEntered";
constexpr char kUserActionEnterPassword[] = "passwordEntered";
constexpr char kUserActionGaiaLoaded[] = "gaiaLoaded";
constexpr char kUserActionUseSAMLAPI[] = "usingSAMLAPI";

constexpr char kLeadingWhitespaceRegex[] = R"(^[\x{0000}-\x{0020}].*)";
constexpr char kTrailingWhitespaceRegex[] = R"(.*[\x{0000}-\x{0020}]$)";

// Returns `true` if the provided string has leading or trailing whitespaces.
// Whitespace is defined as a character with code from '\u0000' to '\u0020'.
bool HasLeadingOrTrailingWhitespaces(const std::string& str) {
  return RE2::FullMatch(str, kLeadingWhitespaceRegex) ||
         RE2::FullMatch(str, kTrailingWhitespaceRegex);
}

absl::optional<SyncTrustedVaultKeys> GetSyncTrustedVaultKeysForUserContext(
    const base::Value::Dict& js_object,
    const std::string& gaia_id) {
  SyncTrustedVaultKeys parsed_keys = SyncTrustedVaultKeys::FromJs(js_object);
  if (parsed_keys.gaia_id() != gaia_id)
    return absl::nullopt;

  return absl::make_optional(std::move(parsed_keys));
}

// Must be kept consistent with ChromeOSSamlApiUsed in enums.xml
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused
enum class ChromeOSSamlApiUsed {
  kNotSamlLogin = 0,
  kSamlApiUsed = 1,
  kSamlApiNotUsed = 2,
  kMaxValue = kSamlApiNotUsed,
};

void RecordAPILogin(bool is_third_party_idp, bool is_api_used) {
  ChromeOSSamlApiUsed login_type;
  if (!is_third_party_idp) {
    login_type = ChromeOSSamlApiUsed::kNotSamlLogin;
  } else if (is_api_used) {
    login_type = ChromeOSSamlApiUsed::kSamlApiUsed;
  } else {
    login_type = ChromeOSSamlApiUsed::kSamlApiNotUsed;
  }
  base::UmaHistogramEnumeration("ChromeOS.SAML.APILogin", login_type);
}

bool ShouldCheckUserTypeBeforeAllowing() {
  if (!features::IsFamilyLinkOnSchoolDeviceEnabled())
    return false;

  CrosSettings* cros_settings = CrosSettings::Get();
  bool family_link_allowed = false;
  cros_settings->GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed,
                            &family_link_allowed);

  return family_link_allowed;
}

AccountId GetAccountId(const std::string& authenticated_email,
                       const std::string& id,
                       const AccountType& account_type) {
  const std::string canonicalized_email =
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(authenticated_email));

  user_manager::KnownUser known_user(g_browser_process->local_state());
  const AccountId account_id =
      known_user.GetAccountId(authenticated_email, id, account_type);

  if (account_id.GetUserEmail() != canonicalized_email) {
    LOG(WARNING) << "Existing user '" << account_id.GetUserEmail()
                 << "' authenticated by alias '" << canonicalized_email << "'.";
  }

  return account_id;
}

user_manager::UserType CalculateUserType(const AccountId& account_id) {
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY)
    return user_manager::USER_TYPE_ACTIVE_DIRECTORY;

  return user_manager::USER_TYPE_REGULAR;
}

}  // namespace

// static
std::string GaiaScreen::GetResultString(Result result) {
  switch (result) {
    case Result::BACK:
      return "Back";
    case Result::CANCEL:
      return "Cancel";
    case Result::ENTERPRISE_ENROLL:
      return "EnterpriseEnroll";
    case Result::START_CONSUMER_KIOSK:
      return "StartConsumerKiosk";
    case Result::LOGIN_SUCCESS:
      return "LoginSuccess";
  }
}

GaiaScreen::GaiaScreen(base::WeakPtr<TView> view,
                       const ScreenExitCallback& exit_callback)
    : BaseScreen(GaiaView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

GaiaScreen::~GaiaScreen() {
  backlights_forced_off_observation_.Reset();
}

void GaiaScreen::LoadOnline(const AccountId& account) {
  if (!view_)
    return;
  auto gaia_path = GaiaView::GaiaPath::kDefault;
  if (!account.empty()) {
    auto* user = user_manager::UserManager::Get()->FindUser(account);
    DCHECK(user);
    if (user && (user->IsChild() || features::IsGaiaReauthEndpointEnabled()))
      gaia_path = GaiaView::GaiaPath::kReauth;
  }
  view_->SetGaiaPath(gaia_path);
  LoadGaiaAsync(account);
}

void GaiaScreen::LoadOnlineForChildSignup() {
  if (!view_)
    return;
  view_->SetGaiaPath(GaiaView::GaiaPath::kChildSignup);
  LoadGaiaAsync(EmptyAccountId());
}

void GaiaScreen::LoadOnlineForChildSignin() {
  if (!view_)
    return;
  view_->SetGaiaPath(GaiaView::GaiaPath::kChildSignin);
  LoadGaiaAsync(EmptyAccountId());
}

void GaiaScreen::ShowAllowlistCheckFailedError() {
  if (!view_)
    return;
  view_->ShowAllowlistCheckFailedError();
}

void GaiaScreen::Reset() {
  if (!view_)
    return;
  view_->SetGaiaPath(GaiaView::GaiaPath::kDefault);
  view_->Reset();
}

void GaiaScreen::ReloadGaiaAuthenticator() {
  if (!view_)
    return;
  view_->ReloadGaiaAuthenticator();
}

void GaiaScreen::ShowImpl() {
  if (!view_)
    return;
  if (!backlights_forced_off_observation_.IsObserving()) {
    backlights_forced_off_observation_.Observe(
        Shell::Get()->backlights_forced_off_setter());
  }
  // Landed on the login screen. No longer skipping enrollment for tests.
  context()->skip_to_login_for_tests = false;
  view_->Show();
  elapsed_timer_ = std::make_unique<base::ElapsedTimer>();
}

void GaiaScreen::HideImpl() {
  if (!view_)
    return;
  view_->SetGaiaPath(GaiaView::GaiaPath::kDefault);
  view_->Hide();
  backlights_forced_off_observation_.Reset();
}

void GaiaScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::BACK);
  } else if (action_id == kUserActionCancel) {
    exit_callback_.Run(Result::CANCEL);
  } else if (action_id == kUserActionStartEnrollment) {
    exit_callback_.Run(Result::ENTERPRISE_ENROLL);
  } else if (action_id == kUserActionReloadDefault) {
    Reset();
    LoadOnline(EmptyAccountId());
  } else if (action_id == kUserActionRetry) {
    LoadOnline(EmptyAccountId());
  } else if (action_id == kUserActionCompleteAuth) {
    CHECK_EQ(10u, args.size());
    const std::string& gaia_id = args[1].GetString();
    const std::string& email = args[2].GetString();
    const std::string& password = args[3].GetString();
    const base::Value::List& scraped_saml_passwords_value = args[4].GetList();
    bool using_saml = args[5].GetBool();
    const base::Value::List& services_list = args[6].GetList();
    bool services_provided = args[7].GetBool();
    const base::Value::Dict& password_attributes = args[8].GetDict();
    const base::Value::Dict& sync_trusted_vault_keys = args[9].GetDict();
    HandleCompleteAuthentication(gaia_id, email, password,
                                 scraped_saml_passwords_value, using_saml,
                                 services_list, services_provided,
                                 password_attributes, sync_trusted_vault_keys);
  } else if (action_id == kUserActionCompleteLoginForTesting) {
    CHECK_EQ(5u, args.size());
    const std::string& gaia_id = args[1].GetString();
    const std::string& email = args[2].GetString();
    const std::string& password = args[3].GetString();
    bool using_saml = args[4].GetBool();
    HandleCompleteLoginForTesting(gaia_id, email, password,  // IN-TEST
                                  using_saml);
  } else if (action_id == kUserActionEnterIdentifier) {
    CHECK_EQ(2u, args.size());
    const std::string& email = args[1].GetString();
    HandleIdentifierEntered(email);
  } else if (action_id == kUserActionEnterPassword) {
    HandlePasswordEntered();
  } else if (action_id == kUserActionGaiaLoaded) {
    HandleGaiaLoaded();
  } else if (action_id == kUserActionUseSAMLAPI) {
    CHECK_EQ(2u, args.size());
    bool is_third_party_idp = args[1].GetBool();
    HandleUsingSAMLAPI(is_third_party_idp);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

bool GaiaScreen::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kStartEnrollment) {
    exit_callback_.Run(Result::ENTERPRISE_ENROLL);
    return true;
  }
  if (action == LoginAcceleratorAction::kEnableConsumerKiosk) {
    exit_callback_.Run(Result::START_CONSUMER_KIOSK);
    return true;
  }
  return false;
}

void GaiaScreen::OnScreenBacklightStateChanged(
    ScreenBacklightState screen_backlight_state) {
  if (screen_backlight_state == ScreenBacklightState::ON)
    return;
  exit_callback_.Run(Result::CANCEL);
}

void GaiaScreen::HandleCompleteAuthentication(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& password_value,
    const base::Value::List& scraped_saml_passwords_value,
    bool using_saml,
    const base::Value::List& services_list,
    bool services_provided,
    const base::Value::Dict& password_attributes,
    const base::Value::Dict& sync_trusted_vault_keys) {
  if (!LoginDisplayHost::default_host())
    return;

  DCHECK(!email.empty());
  DCHECK(!gaia_id.empty());

  if (!using_saml) {
    base::UmaHistogramEnumeration("OOBE.GaiaScreen.SuccessLoginRequests",
                                  login_request_variant_);
    // Report whether the password has characters ignored by Gaia
    // (leading/trailing whitespaces).
    base::UmaHistogramBoolean("OOBE.GaiaScreen.PasswordIgnoredChars",
                              HasLeadingOrTrailingWhitespaces(password_value));
  }
  auto scraped_saml_passwords =
      ::login::ConvertToStringList(scraped_saml_passwords_value);
  const auto services = ::login::ConvertToStringList(services_list);
  auto password = password_value;

  if (IsSamlUserPasswordless()) {
    // In the passwordless case, the user data will be protected by non password
    // based mechanisms. Clear anything that got collected into passwords.
    scraped_saml_passwords.clear();
    password.clear();
  }

  if (using_saml && !using_saml_api_ && !IsSamlUserPasswordless()) {
    RecordScrapedPasswordCount(scraped_saml_passwords.size());
  }

  const AccountId account_id =
      GetAccountId(email, gaia_id, AccountType::GOOGLE);
  // Execute delayed allowlist check that is based on user type. If Gaia done
  // times out and doesn't provide us with services list try to use a saved
  // UserType.
  const user_manager::UserType user_type =
      services_provided
          ? login::GetUsertypeFromServicesString(services)
          : user_manager::UserManager::Get()->GetUserType(account_id);
  if (ShouldCheckUserTypeBeforeAllowing() &&
      !LoginDisplayHost::default_host()->IsUserAllowlisted(account_id,
                                                           user_type)) {
    ShowAllowlistCheckFailedError();
    return;
  }

  // Record amount of time from the moment screen was shown till
  // completeAuthentication signal come. Only for no SAML flow and only during
  // first run in OOBE.
  if (elapsed_timer_ && !using_saml &&
      session_manager::SessionManager::Get()->session_state() ==
          session_manager::SessionState::OOBE) {
    base::UmaHistogramMediumTimes("OOBE.GaiaLoginTime",
                                  elapsed_timer_->Elapsed());
    elapsed_timer_.reset();
  }

  const std::string sanitized_email = gaia::SanitizeEmail(email);
  LoginDisplayHost::default_host()->SetDisplayEmail(sanitized_email);

  OnlineLoginHelper::CompleteLoginCallback complete_login_callback =
      base::BindOnce(&GaiaScreen::OnCompleteLogin, weak_factory_.GetWeakPtr());

  if (password.empty() && !IsSamlUserPasswordless()) {
    CHECK_NE(scraped_saml_passwords.size(), 1u);
    complete_login_callback = base::BindOnce(&GaiaScreen::SAMLConfirmPassword,
                                             weak_factory_.GetWeakPtr(),
                                             std::move(scraped_saml_passwords));
  }

  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          ProfileHelper::GetSigninProfile());
  online_login_helper_ = std::make_unique<OnlineLoginHelper>(
      view_->GetSigninPartitionName(), signin_partition_manager,
      base::BindOnce(&GaiaScreen::OnCookieWaitTimeout,
                     weak_factory_.GetWeakPtr()),
      std::move(complete_login_callback));

  auto user_context = std::make_unique<UserContext>();
  SigninError error;
  if (!login::BuildUserContextForGaiaSignIn(
          user_type, account_id, using_saml, using_saml_api_, password,
          SamlPasswordAttributes::FromJs(password_attributes),
          GetSyncTrustedVaultKeysForUserContext(sync_trusted_vault_keys,
                                                gaia_id),
          *extension_provided_client_cert_usage_observer_, user_context.get(),
          &error)) {
    LoginDisplayHost::default_host()->GetSigninUI()->ShowSigninError(
        error, /*details=*/std::string());
    return;
  }

  online_login_helper_->SetUserContext(std::move(user_context));
  online_login_helper_->RequestCookiesAndCompleteAuthentication();

  view_->OnCompleteAuthentication(sanitized_email);
}

void GaiaScreen::HandleCompleteLoginForTesting(const std::string& gaia_id,
                                               const std::string& typed_email,
                                               const std::string& password,
                                               bool using_saml) {
  VLOG(1) << "HandleCompleteLoginForTesting";
  DCHECK(!typed_email.empty());
  DCHECK(!gaia_id.empty());
  const std::string sanitized_email = gaia::SanitizeEmail(typed_email);
  LoginDisplayHost::default_host()->SetDisplayEmail(sanitized_email);
  const AccountId account_id =
      GetAccountId(typed_email, gaia_id, AccountType::GOOGLE);
  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);

  auto user_context = std::make_unique<UserContext>();
  SigninError error;
  if (!login::BuildUserContextForGaiaSignIn(
          user ? user->GetType() : CalculateUserType(account_id),
          GetAccountId(typed_email, gaia_id, AccountType::GOOGLE), using_saml,
          using_saml_api_, password, SamlPasswordAttributes(),
          /*sync_trusted_vault_keys=*/absl::nullopt,
          *extension_provided_client_cert_usage_observer_, user_context.get(),
          &error)) {
    LoginDisplayHost::default_host()->GetSigninUI()->ShowSigninError(
        error, /*details=*/std::string());
    return;
  }

  OnCompleteLogin(std::move(user_context));
}

void GaiaScreen::HandleIdentifierEntered(const std::string& user_email) {
  // We cannot tell a user type from the identifier, so we delay checking if
  // the account should be allowed.
  if (ShouldCheckUserTypeBeforeAllowing())
    return;

  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (LoginDisplayHost::default_host() &&
      !LoginDisplayHost::default_host()->IsUserAllowlisted(
          known_user.GetAccountId(user_email, std::string() /* id */,
                                  AccountType::UNKNOWN),
          absl::nullopt)) {
    ShowAllowlistCheckFailedError();
  }
}

void GaiaScreen::HandlePasswordEntered() {
  base::UmaHistogramEnumeration("OOBE.GaiaScreen.LoginRequests",
                                login_request_variant_);
}

void GaiaScreen::HandleGaiaLoaded() {
  VLOG(1) << "Gaia finished loading";
  // Recreate the client cert usage observer, in order to track only the certs
  // used during the current sign-in attempt.
  extension_provided_client_cert_usage_observer_ =
      std::make_unique<LoginClientCertUsageObserver>();

  // Clear old storage partitions after a new sign-in page is loaded. All
  // reference to the old storage partitions should be cleared.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          ProfileHelper::GetSigninProfile());
  signin_partition_manager->DisposeOldStoragePartitions();
}

void GaiaScreen::HandleUsingSAMLAPI(bool is_third_party_idp) {
  OnSamlPrincipalsAPIUsed(is_third_party_idp, /*is_api_used=*/true);
}

void GaiaScreen::LoadGaiaAsync(const AccountId& account_id) {
  login_request_variant_ = GaiaView::GaiaLoginVariant::kUnknown;
  if (account_id.is_valid()) {
    login_request_variant_ = GaiaView::GaiaLoginVariant::kOnlineSignin;
  } else {
    if (StartupUtils::IsOobeCompleted() && StartupUtils::IsDeviceOwned()) {
      login_request_variant_ = GaiaView::GaiaLoginVariant::kAddUser;
    } else {
      login_request_variant_ = GaiaView::GaiaLoginVariant::kOobe;
    }
  }
  view_->LoadGaiaAsync(account_id);
}

void GaiaScreen::OnSamlPrincipalsAPIUsed(bool is_third_party_idp,
                                         bool is_api_used) {
  using_saml_api_ = is_api_used;
  // This correctly records the standard GAIA login and SAML flow
  // with Chrome Credentials Passing API used/not used
  RecordAPILogin(is_third_party_idp, is_api_used);
}

void GaiaScreen::RecordScrapedPasswordCount(int password_count) {
  // We are handling scraped passwords here so this is SAML flow without
  // Chrome Credentials Passing API
  OnSamlPrincipalsAPIUsed(/*is_third_party_idp=*/true, /*is_api_used=*/false);
  // Use a histogram that has 11 buckets, one for each of the values in [0, 9]
  // and an overflow bucket at the end.
  base::UmaHistogramExactLinear("ChromeOS.SAML.Scraping.PasswordCountAll",
                                password_count, 11);
}

bool GaiaScreen::IsSamlUserPasswordless() {
  return extension_provided_client_cert_usage_observer_ &&
         extension_provided_client_cert_usage_observer_->ClientCertsWereUsed();
}

void GaiaScreen::OnCookieWaitTimeout() {
  LoginDisplayHost::default_host()->GetSigninUI()->ShowSigninError(
      SigninError::kCookieWaitTimeout, /*details=*/std::string());
}

void GaiaScreen::OnCompleteLogin(std::unique_ptr<UserContext> user_context) {
  context()->extra_factors_auth_session = std::move(user_context);
  exit_callback_.Run(Result::LOGIN_SUCCESS);
}

void GaiaScreen::SAMLConfirmPassword(
    ::login::StringList scraped_saml_passwords,
    std::unique_ptr<UserContext> user_context) {
  // TODO(yunkez): Use exit code and move this logic to wizard controller.
  LoginDisplayHost::default_host()->GetSigninUI()->SAMLConfirmPassword(
      std::move(scraped_saml_passwords), std::move(user_context));
}

}  // namespace ash
