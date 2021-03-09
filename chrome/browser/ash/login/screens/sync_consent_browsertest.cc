// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/test/active_directory_login_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

using testing::Contains;
using testing::Eq;
using testing::UnorderedElementsAreArray;

namespace chromeos {
namespace {

constexpr char kSyncConsent[] = "sync-consent";

const test::UIPath kOverviewDialog = {kSyncConsent,
                                      "syncConsentOverviewDialog"};
const test::UIPath kSplitSettingsDialog = {kSyncConsent,
                                           "splitSettingsSyncConsentDialog"};
const test::UIPath kSettingsSaveAndContinueButton = {
    kSyncConsent, "settingsSaveAndContinueButton"};
const test::UIPath kAcceptButton = {kSyncConsent, "acceptButton"};
const test::UIPath kDeclineButton = {kSyncConsent, "declineButton"};

syncer::SyncUserSettings* GetSyncUserSettings() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  return ProfileSyncServiceFactory::GetForProfile(profile)->GetUserSettings();
}

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
  SyncConsentTest()
      : force_branded_build_(
            WizardController::ForceBrandedBuildForTesting(true)) {}
  ~SyncConsentTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    if (features::IsSplitSettingsSyncEnabled()) {
      expected_consent_ids_ = {
          IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_SUBTITLE,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_NAME,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_DESCRIPTION,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_NAME,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_DECLINE2,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT2,
      };
    } else {
      expected_consent_ids_ = {
          IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_NAME,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
      };
    }

    ReplaceExitCallback();

    // Set up screen to be shown.
    GetSyncConsentScreen()->SetProfileSyncDisabledByPolicyForTesting(false);
    GetSyncConsentScreen()->SetProfileSyncEngineInitializedForTesting(true);
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
    WelcomeScreen* welcome_screen =
        WizardController::default_controller()->GetScreen<WelcomeScreen>();
    test::LanguageReloadObserver observer(welcome_screen);
    test::OobeJS().SelectElementInPath(language,
                                       {"connect", "languageSelect", "select"});
    observer.Wait();
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(SyncConsentScreenView::kScreenId).Wait();
  }

  void ReplaceExitCallback() {
    original_callback_ =
        GetSyncConsentScreen()->get_exit_callback_for_testing();
    GetSyncConsentScreen()->set_exit_callback_for_testing(base::BindRepeating(
        &SyncConsentTest::HandleScreenExit, base::Unretained(this)));
  }

  void LoginToSyncConsentScreen() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    // No need to explicitly show the screen as it is the first one after login.
  }

 protected:
  base::Optional<SyncConsentScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;
  std::vector<int> expected_consent_ids_;

  static SyncConsentScreen* GetSyncConsentScreen() {
    return static_cast<SyncConsentScreen*>(
        WizardController::default_controller()->GetScreen(
            SyncConsentScreenView::kScreenId));
  }

  std::vector<std::string> GetLocalizedExpectedConsentStrings() const {
    std::vector<std::string> result;
    for (const int& id : expected_consent_ids_) {
      result.push_back(GetLocalizedConsentString(id));
    }
    return result;
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void HandleScreenExit(SyncConsentScreen::Result result) {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  SyncConsentScreen::ScreenExitCallback original_callback_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_};

  std::unique_ptr<base::AutoReset<bool>> force_branded_build_;
  DISALLOW_COPY_AND_ASSIGN(SyncConsentTest);
};

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SkippedNotBrandedBuild) {
  auto autoreset = WizardController::ForceBrandedBuildForTesting(false);
  LoginToSyncConsentScreen();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 0);
}

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SkippedSyncDisabledByPolicy) {
  // Set up screen and policy.
  auto autoreset = WizardController::ForceBrandedBuildForTesting(true);
  SyncConsentScreen* screen = GetSyncConsentScreen();
  screen->SetProfileSyncDisabledByPolicyForTesting(true);

  LoginToSyncConsentScreen();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 0);
}

// Tests of the consent recorder with SplitSettingsSync disabled. The
// SplitSettingsSync suite below has its own consent recorder tests.
class SyncConsentRecorderTest : public SyncConsentTest {
 public:
  SyncConsentRecorderTest() {
    features_.InitAndDisableFeature(chromeos::features::kSplitSettingsSync);
  }
  ~SyncConsentRecorderTest() override = default;

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SyncConsentRecorderTest, SyncConsentRecorder) {
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  LoginToSyncConsentScreen();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  test::OobeJS().TapOnPath(kSettingsSaveAndContinueButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_EQ("Accept and continue",
            consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              UnorderedElementsAreArray(expected_consent_ids_));
  EXPECT_EQ(IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
            consent_recorded_waiter.consent_confirmation_id_);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kShow, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.SyncEnabled",
                                       true, 1);
}

class SyncConsentTestWithParams
    : public SyncConsentRecorderTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  SyncConsentTestWithParams() = default;
  ~SyncConsentTestWithParams() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncConsentTestWithParams);
};

IN_PROC_BROWSER_TEST_P(SyncConsentTestWithParams, SyncConsentTestWithLocale) {
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  SwitchLanguage(GetParam());
  LoginToSyncConsentScreen();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().TapOnPath(kSettingsSaveAndContinueButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);

  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_THAT(consent_recorded_waiter.consent_confirmation_string_,
              Eq(GetLocalizedConsentString(
                  IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE)));
}

// "es" tests language switching, "en-GB" checks switching to language varants.
INSTANTIATE_TEST_SUITE_P(All,
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

  OobeScreenExitWaiter waiter(SyncConsentScreenView::kScreenId);

  screen->SetProfileSyncDisabledByPolicyForTesting(true);
  screen->SetProfileSyncEngineInitializedForTesting(GetParam());
  screen->OnStateChanged(nullptr);

  waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyncConsentPolicyDisabledTest,
                         testing::Bool());

// Additional tests of the consent dialog that are only applicable when the
// SplitSettingsSync flag enabled.
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

// Flaky failures on sanitizer builds. https://crbug.com/1054377
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_DefaultFlow DISABLED_DefaultFlow
#else
#define MAYBE_DefaultFlow DefaultFlow
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentSplitSettingsSyncTest, MAYBE_DefaultFlow) {
  LoginToSyncConsentScreen();

  // OS sync is disabled by default.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled));

  // Dialog not completed yet.
  EXPECT_FALSE(prefs->GetBoolean(chromeos::prefs::kSyncOobeCompleted));

  // Wait for content to load.
  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);
  screen->SetProfileSyncDisabledByPolicyForTesting(false);
  screen->SetProfileSyncEngineInitializedForTesting(true);
  screen->OnStateChanged(nullptr);
  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();

  // Dialog is visible.
  test::OobeJS().ExpectVisiblePath(kSplitSettingsDialog);

  // Click the accept button and wait for the JS to C++ callback.
  test::OobeJS().ClickOnPath(kAcceptButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);

  // Consent was recorded for the confirmation button.
  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_EQ("Got it", consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_EQ(IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT2,
            consent_recorded_waiter.consent_confirmation_id_);

  // Consent was recorded for all descriptions, including the confirmation
  // button label.
  std::vector<int> expected_ids = {
      IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_SUBTITLE,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT2,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_DECLINE2,
  };
  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              testing::UnorderedElementsAreArray(expected_ids));

  // OS sync is on.
  EXPECT_TRUE(prefs->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled));

  // Browser sync is on.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_TRUE(settings->IsSyncRequested());
  EXPECT_TRUE(settings->IsFirstSetupComplete());
  EXPECT_TRUE(settings->IsSyncEverythingEnabled());

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kShow, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.UserChoice",
      SyncConsentScreenHandler::UserChoice::kAccepted, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.SyncEnabled",
                                       true, 1);

  // Dialog is completed.
  EXPECT_TRUE(prefs->GetBoolean(chromeos::prefs::kSyncOobeCompleted));
}

// Flaky failures on sanitizer builds. https://crbug.com/1054377
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_DisableSync DISABLED_DisableSync
#else
#define MAYBE_DisableSync DisableSync
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentSplitSettingsSyncTest, MAYBE_DisableSync) {
  LoginToSyncConsentScreen();

  // Wait for content to load.
  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);
  screen->SetProfileSyncDisabledByPolicyForTesting(false);
  screen->SetProfileSyncEngineInitializedForTesting(true);
  screen->OnStateChanged(nullptr);
  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();

  // Click the decline button and wait for the JS to C++ callback.
  test::OobeJS().ClickOnPath(kDeclineButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);

  // OS sync is off.
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled));

  // For historical reasons, browser sync is still on. However, all data types
  // are disabled.
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_TRUE(settings->IsSyncRequested());
  EXPECT_TRUE(settings->IsFirstSetupComplete());
  EXPECT_FALSE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->GetSelectedTypes().Empty());

  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kShow, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.UserChoice",
      SyncConsentScreenHandler::UserChoice::kDeclined, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.SyncEnabled",
                                       false, 1);

  // Dialog is completed.
  EXPECT_TRUE(prefs->GetBoolean(chromeos::prefs::kSyncOobeCompleted));
}

IN_PROC_BROWSER_TEST_F(SyncConsentSplitSettingsSyncTest, LanguageSwitch) {
  SwitchLanguage("es");
  LoginToSyncConsentScreen();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().TapOnPath(kAcceptButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);

  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_THAT(
      consent_recorded_waiter.consent_confirmation_string_,
      Eq(GetLocalizedConsentString(IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT2)));
}

IN_PROC_BROWSER_TEST_F(SyncConsentSplitSettingsSyncTest, LanguageVariant) {
  SwitchLanguage("en-GB");
  LoginToSyncConsentScreen();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().TapOnPath(kAcceptButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);

  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_THAT(
      consent_recorded_waiter.consent_confirmation_string_,
      Eq(GetLocalizedConsentString(IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT2)));
}

IN_PROC_BROWSER_TEST_F(SyncConsentSplitSettingsSyncTest,
                       SkippedNotBrandedBuild) {
  auto autoreset = WizardController::ForceBrandedBuildForTesting(false);
  LoginToSyncConsentScreen();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);

  // OS sync is on.
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_TRUE(settings->IsOsSyncFeatureEnabled());

  // Browser sync is on.
  EXPECT_TRUE(settings->IsSyncRequested());
  EXPECT_TRUE(settings->IsFirstSetupComplete());

  // Dialog is completed.
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(chromeos::prefs::kSyncOobeCompleted));

  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kSkipAndEnableNonBrandedBuild, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.SyncEnabled",
                                       true, 1);
}

IN_PROC_BROWSER_TEST_F(SyncConsentSplitSettingsSyncTest,
                       SkippedSyncDisabledByPolicy) {
  SyncConsentScreen* screen = GetSyncConsentScreen();
  screen->SetProfileSyncDisabledByPolicyForTesting(true);
  LoginToSyncConsentScreen();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);

  // OS sync is off.
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_FALSE(settings->IsOsSyncFeatureEnabled());

  // Browser sync is off.
  EXPECT_FALSE(settings->IsSyncRequested());
  EXPECT_FALSE(settings->IsFirstSetupComplete());

  // Dialog is completed.
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(chromeos::prefs::kSyncOobeCompleted));

  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kSkipPermissionsPolicy, 1);
  // We don't test SyncEnabled because this test fakes the policy disable and
  // the sync engine is still enabled.
}

// Tests for Active Directory accounts, which skip the dialog because they do
// not use sync.
class SyncConsentActiveDirectoryTest : public OobeBaseTest {
 public:
  SyncConsentActiveDirectoryTest() {
    sync_feature_list_.InitAndEnableFeature(
        chromeos::features::kSplitSettingsSync);
  }
  ~SyncConsentActiveDirectoryTest() override = default;

 protected:
  base::test::ScopedFeatureList sync_feature_list_;
  DeviceStateMixin device_state_{
      &mixin_host_,
      DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED};
  ActiveDirectoryLoginMixin ad_login_{&mixin_host_};
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(SyncConsentActiveDirectoryTest, LoginDoesNotStartSync) {
  // Sign in Active Directory user.
  ad_login_.TestLoginVisible();
  ad_login_.SubmitActiveDirectoryCredentials(
      "test-user@locally-managed.localhost", "password");
  test::WaitForPrimaryUserSessionStart();

  // OS sync is off.
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_FALSE(settings->IsOsSyncFeatureEnabled());

  // Browser sync is off.
  EXPECT_FALSE(settings->IsSyncRequested());
  EXPECT_FALSE(settings->IsFirstSetupComplete());

  // Dialog is marked completed (because it was skipped).
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(chromeos::prefs::kSyncOobeCompleted));

  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 0);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kSkipNonGaiaAccount, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.SyncEnabled",
                                       false, 1);
}

// Tests that the SyncConsent screen performs a timezone request so that
// subsequent screens can have a timezone to work with, and that the timezone
// is properly stored in a preference.
class SyncConsentTimezoneOverride : public SyncConsentTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kOobeTimezoneOverrideForTests,
                                    "TimezeonPropagationTest");
    SyncConsentTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(SyncConsentTimezoneOverride, MakesTimezoneRequest) {
  LoginToSyncConsentScreen();
  EXPECT_EQ("TimezeonPropagationTest",
            g_browser_process->local_state()->GetString(
                ::prefs::kSigninScreenTimezone));
}

}  // namespace
}  // namespace chromeos
