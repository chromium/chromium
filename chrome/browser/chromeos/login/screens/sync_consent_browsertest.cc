// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "content/public/test/test_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace {

class ConsentRecordedWaiter
    : public SyncConsentScreen::SyncConsentScreenTestDelegate {
 public:
  ConsentRecordedWaiter() = default;

  void Wait() {
    if (!consent_description_strings_.empty())
      return;

    run_loop_.Run();
  }

  // SyncConsentScreen::SyncConsentScreenTestDelegate
  void OnConsentRecordedIds(SyncConsentScreen::ConsentGiven consent_given,
                            const std::vector<int>& consent_description,
                            int consent_confirmation) override {
    consent_given_ = consent_given;
    consent_description_ids_ = consent_description;
    consent_confirmation_id_ = consent_confirmation;
  }

  void OnConsentRecordedStrings(
      const ::login::StringList& consent_description,
      const std::string& consent_confirmation) override {
    consent_description_strings_ = consent_description;
    consent_confirmation_string_ = consent_confirmation;

    // SyncConsentScreenHandler::SyncConsentScreenHandlerTestDelegate is
    // notified after SyncConsentScreen::SyncConsentScreenTestDelegate, so
    // this is the only place where we need to quit loop.
    run_loop_.Quit();
  }

  SyncConsentScreen::ConsentGiven consent_given_;
  std::vector<int> consent_description_ids_;
  int consent_confirmation_id_;

  ::login::StringList consent_description_strings_;
  std::string consent_confirmation_string_;

  base::RunLoop run_loop_;
};

std::string GetLocalizedConsentString(const int id) {
  std::string sanitized_string =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(id));
  base::ReplaceSubstringsAfterOffset(&sanitized_string, 0, "\u00A0" /* NBSP */,
                                     "&nbsp;");
  return sanitized_string;
}

class SyncConsentTest : public OobeBaseTest {
 public:
  SyncConsentTest() = default;
  ~SyncConsentTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    branded_build_override_ = WizardController::ForceBrandedBuildForTesting();
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }

    OobeBaseTest::TearDownOnMainThread();
  }

  void SwitchLanguage(const std::string& language) {
    const char get_num_reloads[] = "Oobe.getInstance().reloadContentNumEvents_";
    const int prev_reloads = test::OobeJS().GetInt(get_num_reloads);
    test::OobeJS().Evaluate("$('connect').applySelectedLanguage_('" + language +
                            "');");
    const std::string condition =
        base::StringPrintf("%s > %d", get_num_reloads, prev_reloads);
    test::OobeJS().CreateWaiter(condition)->Wait();
  }

  void LoginToSyncConsentScreen() {
    WizardController::default_controller()->SkipToLoginForTesting(
        LoginScreenContext());
    WaitForGaiaPageEvent("ready");
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                  FakeGaiaMixin::kFakeUserPassword,
                                  FakeGaiaMixin::kEmptyUserServices);

    test::CreateOobeScreenWaiter("sync-consent")->Wait();

    // Skip the Assistant opt-in flow screen to avoid it blocking the test.
    auto* screen = static_cast<AssistantOptInFlowScreen*>(
        WizardController::default_controller()->GetScreen(
            AssistantOptInFlowScreenView::kScreenId));
    screen->SetSkipForTesting();
  }

 protected:
  static SyncConsentScreen* GetSyncConsentScreen() {
    return static_cast<SyncConsentScreen*>(
        WizardController::default_controller()->GetScreen(
            SyncConsentScreenView::kScreenId));
  }

  void SyncConsentRecorderTestImpl(
      const std::vector<std::string>& expected_consent_strings,
      const std::string expected_consent_confirmation_string) {
    SyncConsentScreen* screen = GetSyncConsentScreen();
    ConsentRecordedWaiter consent_recorded_waiter;
    screen->SetDelegateForTesting(&consent_recorded_waiter);

    screen->SetProfileSyncDisabledByPolicyForTesting(false);
    screen->SetProfileSyncEngineInitializedForTesting(true);
    screen->OnStateChanged(nullptr);
    test::OobeJS().CreateVisibilityWaiter(true, {"sync-consent-impl"})->Wait();

    test::OobeJS().ExpectVisiblePath(
        {"sync-consent-impl", "syncConsentOverviewDialog"});
    test::OobeJS().TapOnPath(
        {"sync-consent-impl", "settingsSaveAndContinueButton"});
    consent_recorded_waiter.Wait();
    screen->SetDelegateForTesting(nullptr);  // cleanup

    const int expected_consent_confirmation_id =
        IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE;

    EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
              consent_recorded_waiter.consent_given_);
    EXPECT_EQ(expected_consent_strings,
              consent_recorded_waiter.consent_description_strings_);
    EXPECT_EQ(expected_consent_confirmation_string,
              consent_recorded_waiter.consent_confirmation_string_);
    EXPECT_EQ(expected_consent_ids,
              consent_recorded_waiter.consent_description_ids_);
    EXPECT_EQ(expected_consent_confirmation_id,
              consent_recorded_waiter.consent_confirmation_id_);
  }

  std::vector<std::string> GetLocalizedExpectedConsentStrings() const {
    std::vector<std::string> result;
    for (const int& id : expected_consent_ids) {
      result.push_back(GetLocalizedConsentString(id));
    }
    return result;
  }

  const std::vector<int> expected_consent_ids = {
      IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
  };

  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncConsentTest);
};

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SyncConsentRecorder) {
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  LoginToSyncConsentScreen();
  // For En-US we hardcode strings here to catch string issues too.
  const std::vector<std::string> expected_consent_strings(
      {"You're signed in!", "Chrome sync",
       "Your bookmarks, history, passwords, and other settings will be synced "
       "to your Google Account so you can use them on all your devices.",
       "Personalize Google services",
       "Google may use your browsing history to personalize Search, ads, and "
       "other Google services. You can change this anytime at "
       "myaccount.google.com/activitycontrols/search",
       "Review sync options following setup", "Accept and continue"});
  const std::string expected_consent_confirmation_string =
      "Accept and continue";
  SyncConsentRecorderTestImpl(expected_consent_strings,
                              expected_consent_confirmation_string);
}

class SyncConsentTestWithParams
    : public SyncConsentTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  SyncConsentTestWithParams() = default;
  ~SyncConsentTestWithParams() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncConsentTestWithParams);
};

// Consistently timing out on linux  http://crbug.com/1025213
#if defined(OS_LINUX)
#define MAYBE_SyncConsentTestWithLocale DISABLED_SyncConsentTestWithLocale
#else
#define MAYBE_SyncConsentTestWithLocale SyncConsentTestWithLocale
#endif
IN_PROC_BROWSER_TEST_P(SyncConsentTestWithParams,
                       MAYBE_SyncConsentTestWithLocale) {
  LOG(INFO) << "SyncConsentTestWithParams() started with param='" << GetParam()
            << "'";
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  SwitchLanguage(GetParam());
  LoginToSyncConsentScreen();
  const std::vector<std::string> expected_consent_strings =
      GetLocalizedExpectedConsentStrings();
  const std::string expected_consent_confirmation_string =
      GetLocalizedConsentString(
          IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE);
  SyncConsentRecorderTestImpl(expected_consent_strings,
                              expected_consent_confirmation_string);
}

// "es" tests language switching, "en-GB" checks switching to language varants.
INSTANTIATE_TEST_SUITE_P(SyncConsentTestWithParamsImpl,
                         SyncConsentTestWithParams,
                         testing::Values("es", "en-GB"));

// Check that policy-disabled sync does not trigger SyncConsent screen.
//
// We need to check that "disabled by policy" disables SyncConsent screen
// independently from sync engine statis. So we run test twice, both for "sync
// engine not yet initialized" and "sync engine initialized" cases. Therefore
// we use WithParamInterface<bool> here.
class SyncConsentPolicyDisabledTest : public SyncConsentTest,
                                      public testing::WithParamInterface<bool> {
};

IN_PROC_BROWSER_TEST_P(SyncConsentPolicyDisabledTest,
                       SyncConsentPolicyDisabled) {
  LoginToSyncConsentScreen();

  SyncConsentScreen* screen = GetSyncConsentScreen();

  screen->SetProfileSyncDisabledByPolicyForTesting(true);
  screen->SetProfileSyncEngineInitializedForTesting(GetParam());
  screen->OnStateChanged(nullptr);

  // Expect for other screens to be skipped and begin user session.
  test::WaitForPrimaryUserSessionStart();
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         SyncConsentPolicyDisabledTest,
                         testing::Bool());

// Tests of the consent dialog with the SplitSettingsSync flag enabled.
class SyncConsentSplitSettingsSyncTest : public SyncConsentTest {
 public:
  SyncConsentSplitSettingsSyncTest() {
    sync_feature_list_.InitAndEnableFeature(
        chromeos::features::kSplitSettingsSync);
  }
  ~SyncConsentSplitSettingsSyncTest() override = default;

 private:
  base::test::ScopedFeatureList sync_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SyncConsentSplitSettingsSyncTest, DefaultFlow) {
  LoginToSyncConsentScreen();

  // OS sync is disabled by default.
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled));

  // Wait for content to load.
  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);
  screen->SetProfileSyncDisabledByPolicyForTesting(false);
  screen->SetProfileSyncEngineInitializedForTesting(true);
  screen->OnStateChanged(nullptr);
  test::OobeJS().CreateVisibilityWaiter(true, {"sync-consent-impl"})->Wait();

  // Dialog is visible.
  test::OobeJS().ExpectVisiblePath(
      {"sync-consent-impl", "osSyncConsentDialog"});

  // Click the continue button and wait for the JS to C++ callback.
  test::OobeJS().ClickOnPath(
      {"sync-consent-impl", "osSyncAcceptAndContinueButton"});
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);

  // Consent was recorded for the confirmation button.
  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_EQ("Accept and continue",
            consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_EQ(IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
            consent_recorded_waiter.consent_confirmation_id_);

  // Consent was recorded for all descriptions, including the confirmation
  // button label.
  // TODO(jamescook): When PLACEHOLDER strings are replaced, add checks for the
  // correct text and IDs here.
  std::vector<std::string> expected_desc_strings = {"Accept and continue"};
  std::vector<int> expected_desc_ids = {
      IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE};
  EXPECT_EQ(expected_desc_strings,
            consent_recorded_waiter.consent_description_strings_);
  EXPECT_EQ(expected_desc_ids,
            consent_recorded_waiter.consent_description_ids_);

  // Toggle button is on-by-default, so OS sync should be on.
  EXPECT_TRUE(prefs->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled));
}

IN_PROC_BROWSER_TEST_F(SyncConsentSplitSettingsSyncTest, UserCanDisable) {
  LoginToSyncConsentScreen();

  // Wait for content to load.
  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);
  screen->SetProfileSyncDisabledByPolicyForTesting(false);
  screen->SetProfileSyncEngineInitializedForTesting(true);
  screen->OnStateChanged(nullptr);
  test::OobeJS().CreateVisibilityWaiter(true, {"sync-consent-impl"})->Wait();

  // Turn off the toggle.
  test::OobeJS().ClickOnPath({"sync-consent-impl", "enableOsSyncToggle"});

  // Click the continue button and wait for the JS to C++ callback.
  test::OobeJS().ClickOnPath(
      {"sync-consent-impl", "osSyncAcceptAndContinueButton"});
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);

  // User did not consent.
  EXPECT_EQ(SyncConsentScreen::CONSENT_NOT_GIVEN,
            consent_recorded_waiter.consent_given_);

  // OS sync is off.
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled));
}

}  // namespace
}  // namespace chromeos
