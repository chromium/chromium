// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/locale_switch_screen.h"

#include <string>

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"

namespace ash {
namespace {

constexpr char kPeopleApiBaseURL[] =
    "https://people.googleapis.com/v1/people/me";

// Structure from https://developers.google.com/people/api/rest/v1/people
std::string GeneratePeopleApiResponse(const std::string& account_locale) {
  const base::Value::Dict people_api_response = base::Value::Dict().Set(
      "locales", base::Value::List().Append(
                     base::Value::Dict()
                         .Set("metadata", base::Value::Dict().Set(
                                              "primary", base::Value(true)))
                         .Set("value", base::Value(account_locale))));
  return base::WriteJson(people_api_response).value_or("");
}

}  // namespace

class LocaleSwitchScreenBrowserTest : public OobeBaseTest {
 public:
  LocaleSwitchScreenBrowserTest();
  ~LocaleSwitchScreenBrowserTest() override = default;

  LocaleSwitchScreenBrowserTest(const LocaleSwitchScreenBrowserTest&) = delete;
  LocaleSwitchScreenBrowserTest& operator=(
      const LocaleSwitchScreenBrowserTest&) = delete;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void ProceedToLocaleSwitchScreen();
  void SetPeopleAPIResponseLocale(const std::string& account_locale);
  LocaleSwitchScreen::Result WaitForScreenExitResult();

 private:
  // Helper to substitute calls to PeopleAPI to fetch preferred user locale.
  std::unique_ptr<content::URLLoaderInterceptor> people_api_interceptor_;
  base::test::TestFuture<LocaleSwitchScreen::Result> screen_result_waiter_;
  LocaleSwitchScreen::ScreenExitCallback original_callback_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};
};

LocaleSwitchScreenBrowserTest::LocaleSwitchScreenBrowserTest() {}

void LocaleSwitchScreenBrowserTest::SetUpOnMainThread() {
  SetPeopleAPIResponseLocale("en");
  LocaleSwitchScreen* screen = static_cast<LocaleSwitchScreen*>(
      WizardController::default_controller()->screen_manager()->GetScreen(
          LocaleSwitchView::kScreenId));
  original_callback_ = screen->get_exit_callback_for_testing();
  screen->set_exit_callback_for_testing(
      screen_result_waiter_.GetRepeatingCallback());
  fake_gaia_.SetupFakeGaiaForLoginWithDefaults();
  OobeBaseTest::SetUpOnMainThread();
}

void LocaleSwitchScreenBrowserTest::TearDownOnMainThread() {
  people_api_interceptor_.reset();
  OobeBaseTest::TearDownOnMainThread();
}

void LocaleSwitchScreenBrowserTest::SetPeopleAPIResponseLocale(
    const std::string& account_locale) {
  people_api_interceptor_.reset();
  people_api_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&account_locale](
              content::URLLoaderInterceptor::RequestParams* params) {
            bool is_people_api_request =
                params->url_request.url.spec().find(kPeopleApiBaseURL) !=
                std::string::npos;
            if (is_people_api_request) {
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-type: application/json\n\n",
                  GeneratePeopleApiResponse(account_locale),
                  params->client.get());
            }
            return is_people_api_request;
          }));
}

void LocaleSwitchScreenBrowserTest::ProceedToLocaleSwitchScreen() {
  LoginManagerMixin::TestUserInfo user_{AccountId::FromUserEmailGaiaId(
      FakeGaiaMixin::kFakeUserEmail, FakeGaiaMixin::kFakeUserGaiaId)};
  UserContext user_context = LoginManagerMixin::CreateDefaultUserContext(user_);
  user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
  login_manager_mixin_.LoginAsNewRegularUser(user_context);

  // Make sure that all of the capabilities are already loaded after login.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info = identity_manager->FindExtendedAccountInfoByGaiaId(
      FakeGaiaMixin::kFakeUserGaiaId);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.SetAllSupportedCapabilities(false);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  OobeScreenExitWaiter(LocaleSwitchView::kScreenId).Wait();
}

LocaleSwitchScreen::Result
LocaleSwitchScreenBrowserTest::WaitForScreenExitResult() {
  LocaleSwitchScreen::Result result = screen_result_waiter_.Take();
  original_callback_.Run(result);
  return result;
}

IN_PROC_BROWSER_TEST_F(LocaleSwitchScreenBrowserTest,
                       SkipWhenLocaleSetOnWelcomeScreen) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kOobeLocaleChangedOnWelcomeScreen, true);
  ProceedToLocaleSwitchScreen();

  const LocaleSwitchScreen::Result screen_result = WaitForScreenExitResult();
  EXPECT_EQ(screen_result, LocaleSwitchScreen::Result::kNotApplicable);
  EXPECT_EQ(local_state->GetBoolean(prefs::kOobeLocaleChangedOnWelcomeScreen),
            false);
}

IN_PROC_BROWSER_TEST_F(LocaleSwitchScreenBrowserTest, SkipWhenSameLocales) {
  const std::string current_locale = g_browser_process->GetApplicationLocale();
  EXPECT_EQ(current_locale, "en-US");
  SetPeopleAPIResponseLocale(current_locale);

  ProceedToLocaleSwitchScreen();

  const LocaleSwitchScreen::Result screen_result = WaitForScreenExitResult();
  EXPECT_EQ(screen_result, LocaleSwitchScreen::Result::kNoSwitchNeeded);

  EXPECT_EQ(g_browser_process->GetApplicationLocale(), current_locale);
}

IN_PROC_BROWSER_TEST_F(LocaleSwitchScreenBrowserTest,
                       SwitchLocaleWhenGAIAAccountHasDifferentLocale) {
  const std::string new_locale = "fr";
  const std::string current_locale = g_browser_process->GetApplicationLocale();

  EXPECT_EQ(current_locale, "en-US");
  SetPeopleAPIResponseLocale(new_locale);

  ProceedToLocaleSwitchScreen();

  const LocaleSwitchScreen::Result screen_result = WaitForScreenExitResult();
  EXPECT_EQ(screen_result, LocaleSwitchScreen::Result::kSwitchSucceded);

  EXPECT_EQ(g_browser_process->GetApplicationLocale(), new_locale);
}

}  // namespace ash
