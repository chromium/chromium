// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
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
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {
namespace {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::UnorderedElementsAreArray;

constexpr char kSyncConsent[] = "sync-consent";

const test::UIPath kOverviewDialog = {kSyncConsent,
                                      "syncConsentOverviewDialog"};
const test::UIPath kReviewSettingsCheckBox = {kSyncConsent,
                                              "reviewSettingsBox"};
const test::UIPath kAcceptButton = {kSyncConsent, "acceptButton"};
const test::UIPath kDeclineButton = {kSyncConsent, "declineButton"};

syncer::SyncUserSettings* GetSyncUserSettings() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  return SyncServiceFactory::GetForProfile(profile)->GetUserSettings();
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
  std::vector<std::u16string> str_substitute;
  str_substitute.push_back(ui::GetChromeOSDeviceName());
  std::string sanitized_string =
      base::UTF16ToUTF8(base::ReplaceStringPlaceholders(
          l10n_util::GetStringUTF16(id), str_substitute, nullptr));
  base::ReplaceSubstringsAfterOffset(&sanitized_string, 0, "\u00A0" /* NBSP */,
                                     "&nbsp;");
  return sanitized_string;
}

class SyncConsentTest
    : public OobeBaseTest,
      public SyncConsentScreen::SyncConsentScreenExitTestDelegate {
 public:
  SyncConsentTest(const SyncConsentTest&) = delete;
  SyncConsentTest& operator=(const SyncConsentTest&) = delete;

  SyncConsentTest() { login_manager_mixin_.set_session_restore_enabled(); }
  ~SyncConsentTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    if (is_arc_restricted_) {
      expected_consent_ids_ = {
          IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE_WITH_ARC_RESTRICTED,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_DESCRIPTION_WITH_ARC_RESTRICTED,
      };
    } else {
      expected_consent_ids_ = {
          IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE_WITH_DEVICE,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_SUBTITLE_2,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_NAME_2,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_NAME_2,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_DESCRIPTION,
      };
    }

    if (is_minor_user_) {
      // In minor mode, decline and turn on button should be displayed.
      expected_consent_ids_.push_back(IDS_LOGIN_SYNC_CONSENT_SCREEN_DECLINE2);
      expected_consent_ids_.push_back(
          IDS_LOGIN_SYNC_CONSENT_SCREEN_TURN_ON_SYNC);
    } else {
      // In regular mdoe, `review later` checkbox and accept button should be
      // displayed.
      if (is_arc_restricted_) {
        expected_consent_ids_.push_back(
            IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER_ARC_RESTRICTED);
      } else {
        expected_consent_ids_.push_back(
            IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER);
      }
      expected_consent_ids_.push_back(
          IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE);
    }

    SyncConsentScreen::SetSyncConsentScreenExitTestDelegate(this);
    SyncConsentScreen::SetProfileSyncDisabledByPolicyForTesting(false);
  }

  void TearDownOnMainThread() override {
    SyncConsentScreen::SetSyncConsentScreenExitTestDelegate(nullptr);

    OobeBaseTest::TearDownOnMainThread();
  }

  void SwitchLanguage(const std::string& language) {
    WelcomeScreen* welcome_screen =
        WizardController::default_controller()->GetScreen<WelcomeScreen>();
    welcome_screen->UpdateLanguageList();
    test::LanguageReloadObserver observer(welcome_screen);
    test::OobeJS().SelectElementInPath(language,
                                       {"connect", "languageSelect", "select"});
    observer.Wait();
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(SyncConsentScreenView::kScreenId).Wait();
  }

  void LoginAndWaitForSyncConsentScreen() {
    login_manager_mixin_.LoginAsNewRegularUser();

    // If the screen has already exited, don't try to show it again.
    // Although, sync consent screen is not the first screen in the onboarding
    // flow when OobeConsolidatedConsent feature is enabled, it can be reached
    // and skipped after its predecessors are skipped.
    if (features::IsOobeConsolidatedConsentEnabled() && !screen_exited_)
      LoginDisplayHost::default_host()->StartWizard(
          SyncConsentScreenView::kScreenId);

    // Sync Consent screen may skip, so OobeScreenWaiter will not stop. Use
    // custom predicate instead.
    test::TestPredicateWaiter(
        base::BindRepeating(
            [](SyncConsentTest* self) {
              if (self->screen_exited_)
                return true;

              if (!LoginDisplayHost::default_host()->GetOobeUI())
                return false;

              return LoginDisplayHost::default_host()
                         ->GetOobeUI()
                         ->current_screen() == SyncConsentScreenView::kScreenId;
            },
            base::Unretained(this)))
        .Wait();
  }

  // Attempts to log in and show sync consent screen if it is not to be skipped.
  void LoginAndShowSyncConsentScreenWithCapability() {
    LoginAndWaitForSyncConsentScreen();
    SetIsMinorUser(is_minor_user_);
    GetSyncConsentScreen()->SetProfileSyncEngineInitializedForTesting(true);
    if (!GetSyncConsentScreen()->IsProfileSyncDisabledByPolicyForTest() &&
        LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build)
      GetSyncConsentScreen()->OnStateChanged(nullptr);
  }

  void LoginToSyncConsentScreenWithUnknownCapability() {
    LoginAndWaitForSyncConsentScreen();
    GetSyncConsentScreen()->SetProfileSyncEngineInitializedForTesting(true);
    GetSyncConsentScreen()->OnStateChanged(nullptr);
  }

 protected:
  absl::optional<SyncConsentScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;
  std::vector<int> expected_consent_ids_;
  bool is_minor_user_ = false;
  bool is_arc_restricted_ = false;

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
  // SyncConsentScreen::SyncConsentScreenExitTestDelegate
  void OnSyncConsentScreenExit(
      SyncConsentScreen::Result result,
      SyncConsentScreen::ScreenExitCallback& original_callback) override {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    original_callback.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  void SetIsMinorUser(bool is_minor_user) {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    AccountInfo account_info =
        identity_manager->FindExtendedAccountInfoByGaiaId(test::kTestGaiaId);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_offer_extended_chrome_sync_promos(!is_minor_user);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_};

  std::unique_ptr<base::AutoReset<bool>> force_branded_build_;
};

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SkippedNotBrandedBuild) {
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
      false;

  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->defer_oobe_flow_finished_for_tests = true;

  LoginAndShowSyncConsentScreenWithCapability();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 0);
}

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SkippedSyncDisabledByPolicy) {
  // Set up screen and policy.
  SyncConsentScreen::SetProfileSyncDisabledByPolicyForTesting(true);

  LoginAndShowSyncConsentScreenWithCapability();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 0);
}

// Flaky. http://b/260014328
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PRE_AbortedSetup DISABLED_PRE_AbortedSetup
#else
#define MAYBE_PRE_AbortedSetup PRE_AbortedSetup
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentTest, MAYBE_PRE_AbortedSetup) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();
  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
}
#undef MAYBE_PRE_AbortedSetup

// Flaky. http://b/260014328
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_AbortedSetup DISABLED_AbortedSetup
#else
#define MAYBE_AbortedSetup AbortedSetup
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentTest, MAYBE_AbortedSetup) {
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Expect sync everything toggle is on when consent flow is abandoned without
  // user action.
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_TRUE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->IsSyncAllOsTypesEnabled());
}
#undef MAYBE_AbortedSetup

// TODO(crbug.com/1312384): Test failed on linux-chromeos-dbg.
#if !defined(NDEBUG)
#define MAYBE_SyncConsentRecorder DISABLED_SyncConsentRecorder
#elif BUILDFLAG(IS_CHROMEOS)  // TODO(crbug.com/1402468): Test failed on Linux
                              // Chromium OS ASan
#define MAYBE_SyncConsentRecorder DISABLED_SyncConsentRecorder
#else
#define MAYBE_SyncConsentRecorder SyncConsentRecorder
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentTest, MAYBE_SyncConsentRecorder) {
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  test::OobeJS().ExpectHiddenPath(kDeclineButton);
  test::OobeJS().TapOnPath(kAcceptButton);
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

// Tests the different combinations of minor mode and ARC restricted mode.
class SyncConsentTestWithModesParams
    : public SyncConsentTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SyncConsentTestWithModesParams() {
    std::tie(is_minor_user_, is_arc_restricted_) = GetParam();
    if (is_arc_restricted_)
      scoped_feature_list_.InitAndEnableFeature(features::kLacrosSupport);
  }

  SyncConsentTestWithModesParams(const SyncConsentTestWithModesParams&) =
      delete;
  SyncConsentTestWithModesParams& operator=(
      const SyncConsentTestWithModesParams&) = delete;

  ~SyncConsentTestWithModesParams() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1312384): Test failed on linux-chromeos-dbg.
// TODO(crbug.com/1392782): Test failed on MSan/LSan.
#if !defined(NDEBUG) || defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_Accept DISABLED_Accept
#else
#define MAYBE_Accept Accept
#endif
IN_PROC_BROWSER_TEST_P(SyncConsentTestWithModesParams, MAYBE_Accept) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);

  if (is_minor_user_) {
    test::OobeJS().ExpectHiddenPath(kReviewSettingsCheckBox);
  } else {
    test::OobeJS().ExpectVisiblePath(kReviewSettingsCheckBox);
  }

  test::OobeJS().TapOnPath(kAcceptButton);

  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));

  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              UnorderedElementsAreArray(expected_consent_ids_));

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyncConsentTestWithModesParams,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Tests the different combinations of LacrosSupport state and the selection of
// the `Review later` checkbox.
class SyncConsentTestWithReviewParams
    : public SyncConsentTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SyncConsentTestWithReviewParams() {
    std::tie(is_lacros_supported_, is_review_settings_checked_) = GetParam();
    if (is_lacros_supported_) {
      scoped_feature_list_.InitAndEnableFeature(features::kLacrosSupport);
    }
  }

  SyncConsentTestWithReviewParams(const SyncConsentTestWithReviewParams&) =
      delete;
  SyncConsentTestWithReviewParams& operator=(
      const SyncConsentTestWithReviewParams&) = delete;

  ~SyncConsentTestWithReviewParams() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool is_lacros_supported_;
  bool is_review_settings_checked_;
};

// TODO(crbug.com/1311979): Test failed on ChromeOS.
// TODO(crbug.com/1392782): Test failed on chromium/ci/Linux ChromiumOS.
#undef MAYBE_Accept
#if !defined(NDEBUG) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Accept DISABLED_Accept
#else
#define MAYBE_Accept Accept
#endif
IN_PROC_BROWSER_TEST_P(SyncConsentTestWithReviewParams, MAYBE_Accept) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  if (is_review_settings_checked_)
    test::OobeJS().TapOnPath(kReviewSettingsCheckBox);
  test::OobeJS().TapOnPath(kAcceptButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.ReviewFollowingSetup",
      is_review_settings_checked_, 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyncConsentTestWithReviewParams,
                         testing::Combine(testing::Bool(), testing::Bool()));

class SyncConsentTestWithParams
    : public SyncConsentTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  SyncConsentTestWithParams() = default;

  SyncConsentTestWithParams(const SyncConsentTestWithParams&) = delete;
  SyncConsentTestWithParams& operator=(const SyncConsentTestWithParams&) =
      delete;

  ~SyncConsentTestWithParams() override = default;
};

// Disabled due to flakiness: crbug.com/1309452
#if !defined(NDEBUG)
#define MAYBE_SyncConsentTestWithLocale DISABLED_SyncConsentTestWithLocale
#else
#define MAYBE_SyncConsentTestWithLocale SyncConsentTestWithLocale
#endif
IN_PROC_BROWSER_TEST_P(SyncConsentTestWithParams,
                       MAYBE_SyncConsentTestWithLocale) {
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  SwitchLanguage(GetParam());
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().TapOnPath(kAcceptButton);
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
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();

  OobeScreenExitWaiter waiter(SyncConsentScreenView::kScreenId);

  SyncConsentScreen::SetProfileSyncDisabledByPolicyForTesting(true);
  SyncConsentScreen::SetProfileSyncEngineInitializedForTesting(GetParam());
  screen->OnStateChanged(nullptr);

  waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(All, SyncConsentPolicyDisabledTest, testing::Bool());

// Tests for Active Directory accounts, which skip the dialog because they do
// not use sync.
class SyncConsentActiveDirectoryTest : public OobeBaseTest {
 public:
  SyncConsentActiveDirectoryTest() {
    // All tests related to Active Directory login don't make sense when the
    // kChromadAvailable feature is disabled. We also don't need to verify that
    // the device is disabled in that case, because the Chromad disabling
    // feature is already tested in `device_disabling_manager_unittest.cc`.
    scoped_feature_list_.InitAndEnableFeature(features::kChromadAvailable);
  }

  ~SyncConsentActiveDirectoryTest() override = default;

 protected:
  DeviceStateMixin device_state_{
      &mixin_host_,
      DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED};
  ActiveDirectoryLoginMixin ad_login_{&mixin_host_};
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SyncConsentActiveDirectoryTest, LoginDoesNotStartSync) {
  // Sign in Active Directory user.
  ad_login_.TestLoginVisible();
  ad_login_.SubmitActiveDirectoryCredentials(
      "test-user@locally-managed.localhost", "password");
  test::WaitForPrimaryUserSessionStart();

  // Browser sync is off.
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_FALSE(settings->IsSyncRequested());
  EXPECT_FALSE(settings->IsFirstSetupComplete());

  // Dialog is marked completed (because it was skipped).
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSyncOobeCompleted));

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
  LoginAndShowSyncConsentScreenWithCapability();
  EXPECT_EQ("TimezeonPropagationTest",
            g_browser_process->local_state()->GetString(
                ::prefs::kSigninScreenTimezone));
}

class SyncConsentMinorModeTest : public SyncConsentTest {
 public:
  SyncConsentMinorModeTest() { is_minor_user_ = true; }
  ~SyncConsentMinorModeTest() override = default;

 private:
  base::test::ScopedFeatureList sync_feature_list_;
};

// TODO(crbug.com/1312384): Test failed on linux-chromeos-dbg and MSAN and LSAN.
#undef MAYBE_Accept
#if !defined(NDEBUG) || defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_Accept DISABLED_Accept
#else
#define MAYBE_Accept Accept
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentMinorModeTest, MAYBE_Accept) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  test::OobeJS().ExpectVisiblePath(kDeclineButton);
  test::OobeJS().ExpectHiddenPath(kReviewSettingsCheckBox);

  // Expect all data types are disabled for minor users when initialized.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_FALSE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->GetSelectedTypes().Empty());
  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
  EXPECT_TRUE(settings->GetSelectedOsTypes().Empty());

  test::OobeJS().TapOnPath(kAcceptButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  // Expect sync everything toggle is on after user accepted sync consent.
  EXPECT_TRUE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->IsSyncAllOsTypesEnabled());

  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_EQ("Turn on sync",
            consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              UnorderedElementsAreArray(expected_consent_ids_));
  EXPECT_EQ(IDS_LOGIN_SYNC_CONSENT_SCREEN_TURN_ON_SYNC,
            consent_recorded_waiter.consent_confirmation_id_);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kShow, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.IsCapabilityKnown", true, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.IsMinorUser",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.UserChoice",
      SyncConsentScreenHandler::UserChoice::kAccepted, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.SyncEnabled",
                                       true, 1);
}

// TODO(crbug.com/1312384): Test failed on linux-chromeos-dbg.
// TODO(crbug.com/1392782): Test failed on MSan/LSan.
#if !defined(NDEBUG) || defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_Decline DISABLED_Decline
#else
#define MAYBE_Decline Decline
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentMinorModeTest, MAYBE_Decline) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  test::OobeJS().ExpectVisiblePath(kAcceptButton);
  test::OobeJS().ExpectHiddenPath(kReviewSettingsCheckBox);

  // Expect all data types are disabled for minor users when initialized.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_FALSE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->GetSelectedTypes().Empty());
  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
  EXPECT_TRUE(settings->GetSelectedOsTypes().Empty());

  test::OobeJS().TapOnPath(kDeclineButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  // Expect all data types are still disabled.
  EXPECT_FALSE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->GetSelectedTypes().Empty());
  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
  EXPECT_TRUE(settings->GetSelectedOsTypes().Empty());

  EXPECT_EQ(SyncConsentScreen::CONSENT_NOT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_EQ("No thanks", consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              UnorderedElementsAreArray(expected_consent_ids_));
  EXPECT_EQ(IDS_LOGIN_SYNC_CONSENT_SCREEN_DECLINE2,
            consent_recorded_waiter.consent_confirmation_id_);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kShow, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.IsCapabilityKnown", true, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.IsMinorUser",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.UserChoice",
      SyncConsentScreenHandler::UserChoice::kDeclined, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.SyncEnabled",
                                       false, 1);
}

// TODO(crbug.com/1312384): Test failed on linux-chromeos-dbg.
#if !defined(NDEBUG)
#define MAYBE_PRE_AbortedSetup DISABLED_PRE_AbortedSetup
#else
#define MAYBE_PRE_AbortedSetup PRE_AbortedSetup
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentMinorModeTest, MAYBE_PRE_AbortedSetup) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();
  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
}

// TODO(crbug.com/1312384): Test failed on linux-chromeos-dbg.
#if !defined(NDEBUG)
#define MAYBE_AbortedSetup DISABLED_AbortedSetup
#else
#define MAYBE_AbortedSetup AbortedSetup
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentMinorModeTest, MAYBE_AbortedSetup) {
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_NE(profile, nullptr);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  ASSERT_NE(identity_manager, nullptr);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Expect all data types are disabled when consent flow is abandoned without
  // user action.
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  ASSERT_NE(settings, nullptr);
  EXPECT_FALSE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->GetSelectedTypes().Empty());
  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
  EXPECT_TRUE(settings->GetSelectedOsTypes().Empty());
}

IN_PROC_BROWSER_TEST_F(SyncConsentMinorModeTest,
                       AssumeMinorUserWhenUnknownCapability) {
  LoginToSyncConsentScreenWithUnknownCapability();
  WaitForScreenShown();

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  test::OobeJS().ExpectVisiblePath(kDeclineButton);
  test::OobeJS().ExpectHiddenPath(kReviewSettingsCheckBox);

  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.IsCapabilityKnown", false, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.IsMinorUser",
                                       true, 1);
}

class SyncConsentTimeoutTest : public SyncConsentTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeTriggerSyncTimeoutForTests);
    SyncConsentTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(SyncConsentTimeoutTest,
                       SyncEngineInitializationTimeout) {
  auto overviewDialogWaiter =
      test::OobeJS().CreateVisibilityWaiter(true, {kOverviewDialog});

  LoginAndWaitForSyncConsentScreen();
  WaitForScreenShown();

  overviewDialogWaiter->Wait();
}

}  // namespace
}  // namespace ash
