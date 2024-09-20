// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/locale_switch_screen.h"

#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/screens/locale_switch_notification.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {
namespace {

constexpr char kPeopleApiURL[] =
    "https://people.googleapis.com/v1/people/me?personFields=locales";

constexpr base::TimeDelta kWaitTimeout = base::Seconds(5);

class GetLocaleOAuth2PeopleAPICall : public OAuth2ApiCallFlow {
 public:
  GetLocaleOAuth2PeopleAPICall(
      base::OnceCallback<void(std::string)> success_callback,
      base::OnceCallback<void()> failure_callback)
      : success_callback_(std::move(success_callback)),
        failure_callback_(std::move(failure_callback)) {}
  GetLocaleOAuth2PeopleAPICall(const GetLocaleOAuth2PeopleAPICall&) = delete;
  GetLocaleOAuth2PeopleAPICall& operator=(const GetLocaleOAuth2PeopleAPICall&) =
      delete;
  ~GetLocaleOAuth2PeopleAPICall() override = default;

 protected:
  // OAuth2ApiCallFlow:
  GURL CreateApiCallUrl() override { return GURL(kPeopleApiURL); }

  std::string CreateApiCallBody() override { return std::string(""); }

  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override {
    std::string response_body;
    if (body) {
      response_body = std::move(*body);
    }

    std::optional<base::Value> value = base::JSONReader::Read(response_body);
    if (!value || !value->is_dict()) {
      LOG(ERROR) << __func__ << " Bad response format";
      std::move(failure_callback_).Run();
      return;
    }
    base::Value::List* locales_list = value->GetDict().FindList("locales");
    if (!locales_list) {
      LOG(ERROR) << __func__ << " No locales available";
      std::move(failure_callback_).Run();
      return;
    }
    for (const auto& locale_dict : *locales_list) {
      const base::Value::Dict* ld = locale_dict.GetIfDict();
      if (!ld) {
        continue;
      }
      const base::Value::Dict* metadata = ld->FindDict("metadata");
      if (metadata->FindBool("primary")) {
        const std::string* locale = ld->FindString("value");
        std::move(success_callback_).Run(*locale);
        return;
      }
    }
    LOG(ERROR) << __func__ << " No valid locale available";
    std::move(failure_callback_).Run();
  }

  // Called when there is a network error or IsExpectedSuccessCode() returns
  // false. |head| or |body| might be null.
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override {
    LOG(ERROR) << __func__
               << " Failed to get preferred user locale, net_error = "
               << net_error;
    std::move(failure_callback_).Run();
  }

  net::PartialNetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag()
      override {
    return net::DefinePartialNetworkTrafficAnnotation(
        "get_locale_oauth2_people_api_call", "oauth2_api_call_flow", R"(
        semantics {
          sender: "ChromeOS Locale Switch Screen"
          description: "Requests a preferred user locale fromt People API"
          trigger:
            "API call after new user creation on ChromeOS to define user locale"
          user_data: {
            type: ACCESS_TOKEN
          }
          internal {
            contacts {
              email: "cros-oobe@google.com"
            }
          }
          data:
            "User's OAuth2 token"
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-07-11"
        }
        policy {
          setting:
            "This feature cannot be disabled by settings, however the request "
            "is made only for signed-in users."
          chrome_device_policy {
            login_screen_locales {
              login_screen_locales: 'not empty'
            }
          }
        })");
  }

 private:
  base::OnceCallback<void(std::string)> success_callback_;
  base::OnceCallback<void()> failure_callback_;
};

}  // namespace

// static
std::string LocaleSwitchScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kLocaleFetchFailed:
      return "LocaleFetchFailed";
    case Result::kLocaleFetchTimeout:
      return "LocaleFetchTimeout";
    case Result::kNoSwitchNeeded:
      return "NoSwitchNeeded";
    case Result::kSwitchFailed:
      return "SwitchFailed";
    case Result::kSwitchSucceded:
      return "SwitchSucceded";
    case Result::kSwitchDelegated:
      return "SwitchDelegated";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

LocaleSwitchScreen::LocaleSwitchScreen(base::WeakPtr<LocaleSwitchView> view,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(LocaleSwitchView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

LocaleSwitchScreen::~LocaleSwitchScreen() = default;

bool LocaleSwitchScreen::MaybeSkip(WizardContext& wizard_context) {
  if (wizard_context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  // Skip GAIA language sync if user specifically set language through the UI
  // on the welcome screen.
  PrefService* local_state = g_browser_process->local_state();
  if (local_state->GetBoolean(prefs::kOobeLocaleChangedOnWelcomeScreen)) {
    VLOG(1) << "Skipping GAIA language sync because user chose specific"
            << " locale on the Welcome Screen.";
    local_state->ClearPref(prefs::kOobeLocaleChangedOnWelcomeScreen);
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  if (user->HasGaiaAccount()) {
    return false;
  }

  // Switch language if logging into a managed guest session.
  if (user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession()) {
    return false;
  }

  exit_callback_.Run(Result::kNotApplicable);
  return true;
}

void LocaleSwitchScreen::ShowImpl() {
  if (context()->extra_factors_token) {
    session_refresher_ = AuthSessionStorage::Get()->KeepAlive(
        context()->extra_factors_token.value());
  }

  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(user->is_profile_created());
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (user->GetType() == user_manager::UserType::kPublicAccount) {
    locale_ =
        profile->GetPrefs()->GetString(language::prefs::kApplicationLocale);
    SwitchLocale();
    return;
  }

  DCHECK(user->HasGaiaAccount());

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager_) {
    NOTREACHED_IN_MIGRATION();
    exit_callback_.Run(Result::kNotApplicable);
    return;
  }

  CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  refresh_token_loaded_ =
      identity_manager_->HasAccountWithRefreshToken(primary_account_id);

  if (identity_manager_->GetErrorStateOfRefreshTokenForAccount(
          primary_account_id) != GoogleServiceAuthError::AuthErrorNone()) {
    exit_callback_.Run(Result::kLocaleFetchFailed);
    return;
  }

  gaia_id_ = user->GetAccountId().GetGaiaId();
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id_);
  account_capabilities_loaded_ =
      refresh_token_loaded_ &&
      account_info.capabilities.AreAllCapabilitiesKnown();
  if (!account_capabilities_loaded_) {
    identity_manager_observer_.Observe(identity_manager_.get());
  }

  FetchPreferredUserLocaleAndSwitchAsync();

  // Wait for a reasonable time to fetch locale and account capabilities.
  timeout_waiter_.Start(FROM_HERE, kWaitTimeout,
                        base::BindOnce(&LocaleSwitchScreen::OnTimeout,
                                       weak_factory_.GetWeakPtr()));
}

void LocaleSwitchScreen::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (error == GoogleServiceAuthError::AuthErrorNone()) {
    return;
  }
  if (account_info.gaia != gaia_id_) {
    return;
  }
  AbandonPeopleAPICall();
  identity_manager_observer_.Reset();
  timeout_waiter_.Stop();
  exit_callback_.Run(Result::kLocaleFetchFailed);
}

void LocaleSwitchScreen::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (account_info.gaia != gaia_id_) {
    return;
  }
  account_capabilities_loaded_ =
      refresh_token_loaded_ &&
      account_info.capabilities.AreAllCapabilitiesKnown();
  if (!account_capabilities_loaded_) {
    return;
  }

  identity_manager_observer_.Reset();
  if (!locale_.empty()) {
    timeout_waiter_.Stop();
    SwitchLocale();
  }
}

void LocaleSwitchScreen::OnRefreshTokensLoaded() {
  // Account information can only be guaranteed correct after refresh tokens
  // are loaded.
  refresh_token_loaded_ = true;
  OnExtendedAccountInfoUpdated(
      identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id_));
}

void LocaleSwitchScreen::FetchPreferredUserLocaleAndSwitchAsync() {
  // Choose scopes to obtain for the access token.
  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kPeopleApiReadOnlyOAuth2Scope);
  scopes.insert(GaiaConstants::kGoogleUserInfoProfile);
  scopes.insert(GaiaConstants::kProfileLanguageReadOnlyOAuth2Scope);

  // Choose the mode in which to fetch the access token:
  // see AccessTokenFetcher::Mode below for definitions.
  auto mode =
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable;

  // Create the fetcher.
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "LocaleSwitchScreen", identity_manager_, scopes,
          base::BindOnce(&LocaleSwitchScreen::OnAccessTokenRequestCompleted,
                         weak_factory_.GetWeakPtr()),
          mode, signin::ConsentLevel::kSignin);
}

void LocaleSwitchScreen::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // It is safe to destroy |access_token_fetcher_| from this callback.
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << __func__ << " Failed to retrieve a token for an API call: "
               << error.error_message();
    OnRequestFailure();
    return;
  }

  auto* profile = ProfileManager::GetActiveUserProfile();
  get_locale_people_api_call_ = std::make_unique<GetLocaleOAuth2PeopleAPICall>(
      base::BindOnce(&LocaleSwitchScreen::OnLocaleFetched,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&LocaleSwitchScreen::OnRequestFailure,
                     weak_factory_.GetWeakPtr()));
  get_locale_people_api_call_->Start(profile->GetURLLoaderFactory(),
                                     access_token_info.token);
}

void LocaleSwitchScreen::OnLocaleFetched(std::string locale) {
  CHECK(locale_.empty());
  locale_ = std::move(locale);
  if (account_capabilities_loaded_) {
    timeout_waiter_.Stop();
    identity_manager_observer_.Reset();
    SwitchLocale();
  }
}

void LocaleSwitchScreen::OnRequestFailure() {
  timeout_waiter_.Stop();
  identity_manager_observer_.Reset();
  exit_callback_.Run(Result::kLocaleFetchFailed);
}

void LocaleSwitchScreen::SwitchLocale() {
  language::ConvertToActualUILocale(&locale_);

  if (locale_.empty() || locale_ == g_browser_process->GetApplicationLocale()) {
    exit_callback_.Run(Result::kNoSwitchNeeded);
    return;
  }

  // Types of users that have a GAIA account and could be used during the
  // "Add Person" flow.
  static constexpr user_manager::UserType kAddPersonUserTypes[] = {
      user_manager::UserType::kRegular, user_manager::UserType::kChild};
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  // Don't show notification for the ephemeral logins, proceed with the default
  // flow.
  if (!chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin() &&
      context()->is_add_person_flow &&
      base::Contains(kAddPersonUserTypes, user->GetType())) {
    VLOG(1) << "Add Person flow detected, delegating locale switch decision"
            << " to the user.";
    // Delegate language switch to the notification. User will be able to
    // decide whether switch/not switch on their own.
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    locale_util::SwitchLanguageCallback callback(base::BindOnce(
        &LocaleSwitchScreen::OnLanguageChangedNotificationCallback,
        weak_factory_.GetWeakPtr()));
    LocaleSwitchNotification::Show(profile, locale_, std::move(callback));
    exit_callback_.Run(Result::kSwitchDelegated);
    return;
  }

  locale_util::SwitchLanguageCallback callback(
      base::BindOnce(&LocaleSwitchScreen::OnLanguageChangedCallback,
                     weak_factory_.GetWeakPtr()));
  locale_util::SwitchLanguage(
      locale_,
      /*enable_locale_keyboard_layouts=*/false,  // The layouts will be synced
                                                 // instead. Also new user could
                                                 // enable required layouts from
                                                 // the settings.
      /*login_layouts_only=*/false, std::move(callback),
      ProfileManager::GetActiveUserProfile());
}

void LocaleSwitchScreen::HideImpl() {
  // It can happen that we advance to one of the onboarding screens in tests
  // directly without waiting for the LocaleSwitchScreen to properly exit.
  //
  // If `timeout_waiter_` is not running it means that we either already exited
  // the screen in a regular way or `SwitchLocale` is being executed. The latter
  // can happen only in tests and it is explicitly handled in the
  // `LocaleSwitchScreen::OnLanguageChangedCallback`.
  //
  // If `timeout_waiter_` is still running it means that either locale or
  // account capabilities haven't been fetched yet and we need to stop the
  // timer, stop observing `IdentintyManager` and cancel the ongoing request to
  // fetch the locale.
  if (timeout_waiter_.IsRunning()) {
    LOG(WARNING) << __func__
                 << " while LocaleSwitchScreen is still executing requests. "
                    "Ignore if tests advance to next OOBE screens directly.";
    timeout_waiter_.Stop();
    identity_manager_observer_.Reset();
    AbandonPeopleAPICall();
  }
  session_refresher_.reset();
}

void LocaleSwitchScreen::OnLanguageChangedCallback(
    const locale_util::LanguageSwitchResult& result) {
  // Return early when the screen is already hidden for tests. Check comment in
  // `LocaleSwitchScreen::HideImpl` for more information.
  if (is_hidden()) {
    LOG(WARNING) << __func__
                 << " when LocaleSwitchScreen is already hidden. "
                    "Ignore if tests advance to next OOBE screens directly.";
    return;
  }

  if (!result.success) {
    exit_callback_.Run(Result::kSwitchFailed);
    return;
  }

  if (view_) {
    view_->UpdateStrings();
  }
  exit_callback_.Run(Result::kSwitchSucceded);
}

void LocaleSwitchScreen::OnLanguageChangedNotificationCallback(
    const locale_util::LanguageSwitchResult& result) {
  if (!result.success) {
    return;
  }

  if (view_) {
    view_->UpdateStrings();
  }
}

void LocaleSwitchScreen::AbandonPeopleAPICall() {
  access_token_fetcher_.reset();
  get_locale_people_api_call_.reset();
}

void LocaleSwitchScreen::OnTimeout() {
  identity_manager_observer_.Reset();
  if (refresh_token_loaded_ && !locale_.empty()) {
    // We should switch locale if locale is fetched but it timed out while
    // waiting for other account information (e.g. capabilities).
    SwitchLocale();
  } else {
    AbandonPeopleAPICall();
    // If it happens during the tests - something is wrong with the test
    // configuration. Thus making it debug log.
    DLOG(ERROR) << "Timeout of the locale fetch";
    exit_callback_.Run(Result::kLocaleFetchTimeout);
  }
}

}  // namespace ash
