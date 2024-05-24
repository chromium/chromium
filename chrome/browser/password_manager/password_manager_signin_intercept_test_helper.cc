// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_signin_intercept_test_helper.h"

#include <optional>

#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

namespace {

constexpr char kGaiaUsername[] = "username";
constexpr char16_t kGaiaUsername16[] = u"username";
constexpr char kGaiaEmail[] = "username@gmail.com";
constexpr char16_t kGaiaEmail16[] = u"username@gmail.com";
constexpr char kGaiaId[] = "test_gaia_id";

}  // namespace

PasswordManagerSigninInterceptTestHelper::
    PasswordManagerSigninInterceptTestHelper(
        net::test_server::EmbeddedTestServer* https_test_server)
    : https_test_server_(https_test_server) {}

PasswordManagerSigninInterceptTestHelper::
    ~PasswordManagerSigninInterceptTestHelper() = default;

void PasswordManagerSigninInterceptTestHelper::SetUpCommandLine(
    base::CommandLine* command_line) {
  // For the password form to be treated as the Gaia signin page.
  command_line->AppendSwitchASCII(switches::kGaiaUrl,
                                  https_test_server_->base_url().spec());
}

void PasswordManagerSigninInterceptTestHelper::SetUpOnMainThread() {
  // Disable profile creation, so that only profile switch interception can
  // trigger.
  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);
}

// Pre-populates the password store with Gaia credentials.
void PasswordManagerSigninInterceptTestHelper::StoreGaiaCredentials(
    scoped_refptr<password_manager::TestPasswordStore> password_store) {
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = GaiaUrls::GetInstance()->gaia_url().spec();
  signin_form.username_value = kGaiaUsername16;
  signin_form.password_value = u"pw";
  password_store->AddLogin(signin_form);
}

void PasswordManagerSigninInterceptTestHelper::NavigateToGaiaSigninPage(
    content::WebContents* contents) {
  std::string path = "/password/password_form.html";
  GURL https_url(https_test_server_->GetURL(path));
  DCHECK(https_url.SchemeIs(url::kHttpsScheme));
  DCHECK(gaia::HasGaiaSchemeHostPort(https_url));

  PasswordsNavigationObserver navigation_observer(contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(chrome::FindBrowserWithTab(contents),
                                           https_url));
  ASSERT_TRUE(navigation_observer.Wait());
}

// Create another profile with the same Gaia account, so that the profile
// switch promo can be shown.
void PasswordManagerSigninInterceptTestHelper::SetupProfilesForInterception(
    Profile* current_profile) {
  // Add a profile in the cache (simulate the profile on disk).
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage* profile_storage =
      &profile_manager->GetProfileAttributesStorage();
  const base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"TestProfileName";
  params.gaia_id = kGaiaId;
  params.user_name = kGaiaEmail16;
  profile_storage->AddProfile(std::move(params));

  // Check that the signin qualifies for interception.
  std::optional<SigninInterceptionHeuristicOutcome> outcome =
      GetSigninInterceptor(current_profile)
          ->GetHeuristicOutcome(
              /*is_new_account=*/true, /*is_sync_signin=*/false, kGaiaUsername);
  DCHECK(outcome.has_value());
  DCHECK(SigninInterceptionHeuristicOutcomeIsSuccess(*outcome));
}

CoreAccountId PasswordManagerSigninInterceptTestHelper::AddGaiaAccountToProfile(
    Profile* profile,
    const std::string& email,
    const std::string& gaia_id) {
  auto* accounts_mutator =
      IdentityManagerFactory::GetForProfile(profile)->GetAccountsMutator();
  return accounts_mutator->AddOrUpdateAccount(
      gaia_id, email, "refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
}

DiceWebSigninInterceptor*
PasswordManagerSigninInterceptTestHelper::GetSigninInterceptor(
    Profile* profile) {
  return DiceWebSigninInterceptorFactory::GetForProfile(profile);
}

std::string PasswordManagerSigninInterceptTestHelper::gaia_username() const {
  return kGaiaUsername;
}

std::string PasswordManagerSigninInterceptTestHelper::gaia_email() const {
  return kGaiaEmail;
}

std::string PasswordManagerSigninInterceptTestHelper::gaia_id() const {
  return kGaiaId;
}
