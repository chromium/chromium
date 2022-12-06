// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/hash/sha1.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_arc_tos_mixin.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/test/webview_content_extractor.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

using ::consent_auditor::FakeConsentAuditor;
using ::sync_pb::UserConsentTypes;
using ::testing::_;
using ::testing::ElementsAre;
using ArcPlayTermsOfServiceConsent =
    ::sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using ArcBackupAndRestoreConsent =
    ::sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent;
using ArcGoogleLocationServiceConsent =
    ::sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent;

const char kManagedUser[] = "user@example.com";
const char kManagedGaiaID[] = "33333";

constexpr char kConsolidatedConsentId[] = "consolidated-consent";

// Loaded Dialog
const test::UIPath kLoadedDialog = {kConsolidatedConsentId, "loadedDialog"};
const test::UIPath kGoogleEulaLinkArcDisabled = {kConsolidatedConsentId,
                                                 "googleEulaLinkArcDisabled"};
const test::UIPath kCrosEulaLinkArcDisabled = {kConsolidatedConsentId,
                                               "crosEulaLinkArcDisabled"};
const test::UIPath kGoogleEulaLink = {kConsolidatedConsentId, "googleEulaLink"};
const test::UIPath kCrosEulaLink = {kConsolidatedConsentId, "crosEulaLink"};
const test::UIPath kArcTosLink = {kConsolidatedConsentId, "arcTosLink"};
const test::UIPath kPrivacyPolicyLink = {kConsolidatedConsentId,
                                         "privacyPolicyLink"};

const test::UIPath kUsageStats = {kConsolidatedConsentId, "usageStats"};
const test::UIPath kUsageStatsToggle = {kConsolidatedConsentId, "usageOptin"};
const test::UIPath kUsageLearnMoreLink = {kConsolidatedConsentId,
                                          "usageLearnMore"};
const test::UIPath kUsageLearnMorePopUp = {kConsolidatedConsentId,
                                           "usageLearnMorePopUp"};
const test::UIPath kUsageLearnMorePopUpClose = {
    kConsolidatedConsentId, "usageLearnMorePopUp", "closeButton"};

const test::UIPath kBackup = {kConsolidatedConsentId, "backup"};
const test::UIPath kBackupToggle = {kConsolidatedConsentId, "backupOptIn"};
const test::UIPath kBackupLearnMoreLink = {kConsolidatedConsentId,
                                           "backupLearnMore"};
const test::UIPath kBackupLearnMorePopUp = {kConsolidatedConsentId,
                                            "backupLearnMorePopUp"};
const test::UIPath kBackupLearnMorePopUpClose = {
    kConsolidatedConsentId, "backupLearnMorePopUp", "closeButton"};

const test::UIPath kRecovery = {kConsolidatedConsentId, "recovery"};

const test::UIPath kLocation = {kConsolidatedConsentId, "location"};
const test::UIPath kLocationToggle = {kConsolidatedConsentId, "locationOptIn"};
const test::UIPath kLocationLearnMoreLink = {kConsolidatedConsentId,
                                             "locationLearnMore"};
const test::UIPath kLocationLearnMorePopUp = {kConsolidatedConsentId,
                                              "locationLearnMorePopUp"};
const test::UIPath kLocationLearnMorePopUpClose = {
    kConsolidatedConsentId, "locationLearnMorePopUp", "closeButton"};

const test::UIPath kFooter = {kConsolidatedConsentId, "footer"};
const test::UIPath kFooterLearnMoreLink = {kConsolidatedConsentId,
                                           "footerLearnMore"};
const test::UIPath kFooterLearnMorePopUp = {kConsolidatedConsentId,
                                            "footerLearnMorePopUp"};
const test::UIPath kFooterLearnMorePopUpClose = {
    kConsolidatedConsentId, "footerLearnMorePopUp", "closeButton"};
const test::UIPath kAcceptButton = {kConsolidatedConsentId, "acceptButton"};
const test::UIPath kReadMoreButton = {kConsolidatedConsentId, "loadedDialog",
                                      "readMoreButton"};

// Google EUlA Dialog
const test::UIPath kGoogleEulaDialog = {kConsolidatedConsentId,
                                        "googleEulaDialog"};
const test::UIPath kGoogleEulaWebview = {
    kConsolidatedConsentId, "consolidatedConsentGoogleEulaWebview"};
const test::UIPath kGoogleEulaOkButton = {kConsolidatedConsentId,
                                          "googleEulaOkButton"};

// CROS EULA Dialog
const test::UIPath kCrosEulaDialog = {kConsolidatedConsentId, "crosEulaDialog"};
const test::UIPath kCrosEulaWebview = {kConsolidatedConsentId,
                                       "consolidatedConsentCrosEulaWebview"};
const test::UIPath kCrosEulaOkButton = {kConsolidatedConsentId,
                                        "crosEulaOkButton"};

// ARC ToS Dialog
const test::UIPath kArcTosDialog = {kConsolidatedConsentId, "arcTosDialog"};
const test::UIPath kArcTosWebview = {kConsolidatedConsentId,
                                     "consolidatedConsentArcTosWebview"};
const test::UIPath kArcTosOkButton = {kConsolidatedConsentId, "arcTosOkButton"};

// Privacy Policy Dialog
const test::UIPath kPrivacyPolicyDialog = {kConsolidatedConsentId,
                                           "privacyPolicyDialog"};
const test::UIPath kPrivacyPolicyWebview = {
    kConsolidatedConsentId, "consolidatedConsentPrivacyPolicyWebview"};
const test::UIPath kPrivacyPolicyOkButton = {kConsolidatedConsentId,
                                             "privacyOkButton"};

// WebViewLoader histograms
inline constexpr char kCrosEulaWebviewFirstLoadResult[] =
    "OOBE.WebViewLoader.FirstLoadResult.ConsolidatedConsentCrosEulaWebview";
inline constexpr char kGoogleEulaWebviewFirstLoadResult[] =
    "OOBE.WebViewLoader.FirstLoadResult.ConsolidatedConsentGoogleEulaWebview";
inline constexpr char kArcTosWebviewFirstLoadResult[] =
    "OOBE.WebViewLoader.FirstLoadResult.ConsolidatedConsentArcTosWebview";
inline constexpr char kPrivacyPolicyFirstLoadResult[] =
    "OOBE.WebViewLoader.FirstLoadResult."
    "ConsolidatedConsentPrivacyPolicyWebview";
inline constexpr char kRecoveryOptInResultHistogram[] =
    "OOBE.ConsolidatedConsentScreen.RecoveryOptInResult";

ArcPlayTermsOfServiceConsent BuildArcPlayTermsOfServiceConsent(
    const std::string& tos_content) {
  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(sync_pb::UserConsentTypes::GIVEN);
  play_consent.set_confirmation_grd_id(
      IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETUP);
  play_consent.set_play_terms_of_service_hash(
      base::SHA1HashString(tos_content));
  play_consent.set_play_terms_of_service_text_length(tos_content.length());
  return play_consent;
}

ArcBackupAndRestoreConsent BuildArcBackupAndRestoreConsent(bool accepted) {
  ArcBackupAndRestoreConsent backup_and_restore_consent;
  backup_and_restore_consent.set_confirmation_grd_id(
      IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
  backup_and_restore_consent.add_description_grd_ids(
      IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_TITLE);
  backup_and_restore_consent.add_description_grd_ids(
      IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN);
  backup_and_restore_consent.set_status(accepted ? UserConsentTypes::GIVEN
                                                 : UserConsentTypes::NOT_GIVEN);

  return backup_and_restore_consent;
}

ArcGoogleLocationServiceConsent BuildArcGoogleLocationServiceConsent(
    bool accepted) {
  ArcGoogleLocationServiceConsent location_service_consent;
  location_service_consent.set_confirmation_grd_id(
      IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
  location_service_consent.add_description_grd_ids(
      IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_TITLE);
  location_service_consent.add_description_grd_ids(
      IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN);
  location_service_consent.set_status(accepted ? UserConsentTypes::GIVEN
                                               : UserConsentTypes::NOT_GIVEN);
  return location_service_consent;
}
}  // namespace

// Regular user flow with ARC not enabled
class ConsolidatedConsentScreenTest : public OobeBaseTest {
 public:
  enum class UserAction {
    kAcceptButtonClicked = 0,
    kBackDemoButtonClicked = 1,
    kGoogleEulaLinkClicked = 2,
    kCrosEulaLinkClicked = 3,
    kArcTosLinkClicked = 4,
    kPrivacyPolicyLinkClicked = 5,
    kUsageOptinLearnMoreClicked = 6,
    kBackupOptinLearnMoreClicked = 7,
    kLocationOptinLearnMoreClicked = 8,
    kFooterLearnMoreClicked = 9,
    kErrorStepRetryButtonClicked = 10,
    kMaxValue = kErrorStepRetryButtonClicked,
  };

  ConsolidatedConsentScreenTest() {
    feature_list_.InitAndEnableFeature(features::kOobeConsolidatedConsent);
  }

  void SetUpOnMainThread() override {
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    // Override the screen exit callback with our own method.
    ConsolidatedConsentScreen* screen =
        WizardController::default_controller()
            ->GetScreen<ConsolidatedConsentScreen>();

    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(
        base::BindRepeating(&ConsolidatedConsentScreenTest::HandleScreenExit,
                            base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void LoginAsRegularUser() { login_manager_mixin_.LoginAsNewRegularUser(); }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  absl::optional<ConsolidatedConsentScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;

  std::vector<base::Bucket> GetAllRecordedUserActions() {
    return histogram_tester_.GetAllSamples(
        "OOBE.ConsolidatedConsentScreen.UserActions");
  }

 protected:
  void HandleScreenExit(ConsolidatedConsentScreen::Result result) {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  ConsolidatedConsentScreen::ScreenExitCallback original_callback_;
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  FakeEulaMixin fake_eula_{&mixin_host_, embedded_test_server()};
};

// For regular users with ARC disabled, only usage stats opt-in is visible
// and the toggle is enabled.
IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenTest, OptinsVisiblity) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ExpectVisiblePath(kUsageStats);
  test::OobeJS().ExpectEnabledPath(kUsageStatsToggle);
  test::OobeJS().ExpectHiddenPath(kBackup);
  test::OobeJS().ExpectHiddenPath(kRecovery);
  test::OobeJS().ExpectHiddenPath(kLocation);
  test::OobeJS().ExpectHiddenPath(kFooter);
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenTest, GoogleEula) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();
  test::OobeJS().ClickOnPath(kGoogleEulaLinkArcDisabled);
  test::OobeJS().CreateVisibilityWaiter(true, kGoogleEulaDialog)->Wait();
  const std::string webview_contents =
      test::GetWebViewContents(kGoogleEulaWebview);
  EXPECT_TRUE(webview_contents.find(FakeEulaMixin::kFakeOnlineEula) !=
              std::string::npos);
  test::OobeJS().ClickOnPath(kGoogleEulaOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  EXPECT_THAT(GetAllRecordedUserActions(),
              ElementsAre(base::Bucket(
                  static_cast<int>(UserAction::kGoogleEulaLinkClicked), 1)));
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenTest, CrosEula) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ClickOnPath(kCrosEulaLinkArcDisabled);
  test::OobeJS().CreateVisibilityWaiter(true, kCrosEulaDialog)->Wait();

  const std::string webview_contents =
      test::GetWebViewContents(kCrosEulaWebview);
  EXPECT_TRUE(webview_contents.find(FakeEulaMixin::kFakeOnlineEula) !=
              std::string::npos);

  test::OobeJS().ClickOnPath(kCrosEulaOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  EXPECT_THAT(GetAllRecordedUserActions(),
              ElementsAre(base::Bucket(
                  static_cast<int>(UserAction::kCrosEulaLinkClicked), 1)));
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenTest, Accept) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAcceptButton)->Wait();
  test::OobeJS().ClickOnPath(kAcceptButton);
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            ConsolidatedConsentScreen::Result::ACCEPTED);

  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime.Consolidated-consent", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepShownStatus.Consolidated-consent", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Consolidated-consent."
      "AcceptedRegular",
      1);

  histogram_tester_.ExpectTotalCount(kRecoveryOptInResultHistogram, 1);
  histogram_tester_.ExpectTotalCount(kGoogleEulaWebviewFirstLoadResult, 1);
  histogram_tester_.ExpectTotalCount(kCrosEulaWebviewFirstLoadResult, 1);

  // ARC is not available, ARC ToS and privacy policy will not be loaded.
  histogram_tester_.ExpectTotalCount(kArcTosWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kPrivacyPolicyFirstLoadResult, 0);

  EXPECT_THAT(GetAllRecordedUserActions(),
              ElementsAre(base::Bucket(
                  static_cast<int>(UserAction::kAcceptButtonClicked), 1)));
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenTest, LearnMore) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ClickOnPath(kUsageLearnMoreLink);
  test::OobeJS().ExpectDialogOpen(kUsageLearnMorePopUp);
  test::OobeJS().ClickOnPath(kUsageLearnMorePopUpClose);
  test::OobeJS().ExpectDialogClosed(kUsageLearnMorePopUp);

  EXPECT_THAT(
      GetAllRecordedUserActions(),
      ElementsAre(base::Bucket(
          static_cast<int>(UserAction::kUsageOptinLearnMoreClicked), 1)));
}

class ConsolidatedConsentScreenArcEnabledTest
    : public ConsolidatedConsentScreenTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    ConsolidatedConsentScreenTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    // Enable ARC for testing.
    arc::ArcServiceLauncher::Get()->ResetForTesting();
    ConsolidatedConsentScreenTest::SetUpOnMainThread();
  }

  FakeArcTosMixin fake_arc_tos_{&mixin_host_, embedded_test_server()};
};

// For regular users with ARC enabled, all ARC opt-ins are visible and the
// toggles are enabled. Recovery service availability would depend on feature
// flag.
IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenArcEnabledTest,
                       OptinsVisiblity) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ExpectVisiblePath(kUsageStats);
  test::OobeJS().ExpectEnabledPath(kUsageStatsToggle);
  test::OobeJS().ExpectVisiblePath(kBackup);
  test::OobeJS().ExpectEnabledPath(kBackupToggle);
  test::OobeJS().ExpectHiddenPath(kRecovery);
  test::OobeJS().ExpectVisiblePath(kLocation);
  test::OobeJS().ExpectEnabledPath(kLocationToggle);

  test::OobeJS().ExpectVisiblePath(kFooter);
}

// Make sure that EULA links in the terms description for the ARC Enabled
// shows the correct dialogs.
IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenArcEnabledTest, EULA) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ClickOnPath(kGoogleEulaLink);
  test::OobeJS().CreateVisibilityWaiter(true, kGoogleEulaDialog)->Wait();
  test::OobeJS().ClickOnPath(kGoogleEulaOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ClickOnPath(kCrosEulaLink);
  test::OobeJS().CreateVisibilityWaiter(true, kCrosEulaDialog)->Wait();
  test::OobeJS().ClickOnPath(kCrosEulaOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  EXPECT_THAT(
      GetAllRecordedUserActions(),
      ElementsAre(
          base::Bucket(static_cast<int>(UserAction::kGoogleEulaLinkClicked), 1),
          base::Bucket(static_cast<int>(UserAction::kCrosEulaLinkClicked), 1)));
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenArcEnabledTest, ArcToS) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ClickOnPath(kArcTosLink);
  test::OobeJS().CreateVisibilityWaiter(true, kArcTosDialog)->Wait();

  const std::string webview_contents = test::GetWebViewContents(kArcTosWebview);
  EXPECT_TRUE(webview_contents.find(fake_arc_tos_.GetArcTosContent()) !=
              std::string::npos);

  test::OobeJS().ClickOnPath(kArcTosOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();
  EXPECT_THAT(GetAllRecordedUserActions(),
              ElementsAre(base::Bucket(
                  static_cast<int>(UserAction::kArcTosLinkClicked), 1)));
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenArcEnabledTest, PrivacyPolicy) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ClickOnPath(kPrivacyPolicyLink);
  test::OobeJS().CreateVisibilityWaiter(true, kPrivacyPolicyDialog)->Wait();

  const std::string webview_contents =
      test::GetWebViewContents(kPrivacyPolicyWebview);
  EXPECT_TRUE(webview_contents.find(fake_arc_tos_.GetPrivacyPolicyContent()) !=
              std::string::npos);

  test::OobeJS().ClickOnPath(kPrivacyPolicyOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();
  EXPECT_THAT(GetAllRecordedUserActions(),
              ElementsAre(base::Bucket(
                  static_cast<int>(UserAction::kPrivacyPolicyLinkClicked), 1)));
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenArcEnabledTest,
                       ArcLearnMoreLinks) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ClickOnPath(kBackupLearnMoreLink);
  test::OobeJS().ExpectDialogOpen(kBackupLearnMorePopUp);
  test::OobeJS().ClickOnPath(kBackupLearnMorePopUpClose);
  test::OobeJS().ExpectDialogClosed(kBackupLearnMorePopUp);

  test::OobeJS().ClickOnPath(kLocationLearnMoreLink);
  test::OobeJS().ExpectDialogOpen(kLocationLearnMorePopUp);
  test::OobeJS().ClickOnPath(kLocationLearnMorePopUpClose);
  test::OobeJS().ExpectDialogClosed(kLocationLearnMorePopUp);

  test::OobeJS().ClickOnPath(kFooterLearnMoreLink);
  test::OobeJS().ExpectDialogOpen(kFooterLearnMorePopUp);
  test::OobeJS().ClickOnPath(kFooterLearnMorePopUpClose);
  test::OobeJS().ExpectDialogClosed(kFooterLearnMorePopUp);

  EXPECT_THAT(
      GetAllRecordedUserActions(),
      ElementsAre(
          base::Bucket(
              static_cast<int>(UserAction::kBackupOptinLearnMoreClicked), 1),
          base::Bucket(
              static_cast<int>(UserAction::kLocationOptinLearnMoreClicked), 1),
          base::Bucket(static_cast<int>(UserAction::kFooterLearnMoreClicked),
                       1)));
}

// There are two toggles for enabling/disabling ARC backup restore and
// ARC location service. This parameterized test executes all 4 combinations
// of enabled/disabled states and checks that advancing to the next screen by
// accepting.
class ConsolidatedConsentScreenParametrizedTest
    : public ConsolidatedConsentScreenArcEnabledTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    std::tie(accept_backup_restore_, accept_location_service_) = GetParam();
    ConsolidatedConsentScreenArcEnabledTest::SetUp();
  }

  // Common routine that enables/disables toggles based on test parameters.
  // `play_consent`, `backup_and_restore_consent` and `location_service_consent`
  // are the expected consents recordings.
  void AdvanceNextScreenWithExpectations(
      ArcPlayTermsOfServiceConsent play_consent,
      ArcBackupAndRestoreConsent backup_and_restore_consent,
      ArcGoogleLocationServiceConsent location_service_consent) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    FakeConsentAuditor* auditor = static_cast<FakeConsentAuditor*>(
        ConsentAuditorFactory::GetInstance()->SetTestingFactoryAndUse(
            profile, base::BindRepeating(&BuildFakeConsentAuditor)));

    if (!accept_backup_restore_)
      test::OobeJS().ClickOnPath(kBackupToggle);

    if (!accept_location_service_)
      test::OobeJS().ClickOnPath(kLocationToggle);

    EXPECT_CALL(*auditor, RecordArcPlayConsent(
                              testing::_,
                              consent_auditor::ArcPlayConsentEq(play_consent)));

    EXPECT_CALL(*auditor,
                RecordArcBackupAndRestoreConsent(
                    testing::_, consent_auditor::ArcBackupAndRestoreConsentEq(
                                    backup_and_restore_consent)));

    EXPECT_CALL(
        *auditor,
        RecordArcGoogleLocationServiceConsent(
            testing::_, consent_auditor::ArcGoogleLocationServiceConsentEq(
                            location_service_consent)));
    test::OobeJS().CreateVisibilityWaiter(true, kAcceptButton)->Wait();
    test::OobeJS().ClickOnPath(kAcceptButton);
  }

 protected:
  bool accept_backup_restore_;
  bool accept_location_service_;
};

// Tests that clicking on "Accept" button records the expected consents.
// When TOS are accepted we should also record whether backup restores and
// location services are enabled.
IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenParametrizedTest, ClickAccept) {
  fake_arc_tos_.set_serve_tos_with_privacy_policy_footer(false);
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  ArcPlayTermsOfServiceConsent play_consent =
      BuildArcPlayTermsOfServiceConsent(fake_arc_tos_.GetArcTosContent());
  ArcBackupAndRestoreConsent backup_and_restore_consent =
      BuildArcBackupAndRestoreConsent(accept_backup_restore_);
  ArcGoogleLocationServiceConsent location_service_consent =
      BuildArcGoogleLocationServiceConsent(accept_location_service_);

  AdvanceNextScreenWithExpectations(play_consent, backup_and_restore_consent,
                                    location_service_consent);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_, ConsolidatedConsentScreen::Result::ACCEPTED);

  histogram_tester_.ExpectTotalCount(kGoogleEulaWebviewFirstLoadResult, 1);
  histogram_tester_.ExpectTotalCount(kCrosEulaWebviewFirstLoadResult, 1);

  // ARC is available, ARC ToS and privacy policy will be loaded.
  histogram_tester_.ExpectTotalCount(kArcTosWebviewFirstLoadResult, 1);
  histogram_tester_.ExpectTotalCount(kPrivacyPolicyFirstLoadResult, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ConsolidatedConsentScreenParametrizedTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class ConsolidatedConsentScreenManagedUserTest
    : public ConsolidatedConsentScreenArcEnabledTest {
 public:
  enum class ArcManagedOptin {
    kManagedDisabled,
    kNotManaged,
    kManagedEnabled,
  };

  void LoginManagedUser() {
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
    login_manager_mixin_.LoginWithDefaultContext(managed_user_);
  }

  void SetUpArcEnabledPolicy() {
    std::unique_ptr<ScopedUserPolicyUpdate> scoped_user_policy_update =
        user_policy_mixin_.RequestPolicyUpdate();
    scoped_user_policy_update->policy_payload()
        ->mutable_arcenabled()
        ->set_value(true);
  }

  void SetUpManagedOptIns(ArcManagedOptin backup_opt_in,
                          ArcManagedOptin location_opt_in) {
    std::unique_ptr<ScopedUserPolicyUpdate> scoped_user_policy_update =
        user_policy_mixin_.RequestPolicyUpdate();
    scoped_user_policy_update->policy_payload()
        ->mutable_arcbackuprestoreserviceenabled()
        ->set_value(int(backup_opt_in));
    scoped_user_policy_update->policy_payload()
        ->mutable_arcgooglelocationservicesenabled()
        ->set_value(int(location_opt_in));
  }

  void CheckTogglesState(ArcManagedOptin backup_opt_in,
                         ArcManagedOptin location_opt_in) {
    test::OobeJS().ExpectVisiblePath(kBackup);
    test::OobeJS().ExpectVisiblePath(kLocation);

    switch (backup_opt_in) {
      case ArcManagedOptin::kManagedDisabled:
        test::OobeJS().ExpectDisabledPath(kBackupToggle);
        test::OobeJS().ExpectHasNoAttribute("checked", kBackupToggle);
        break;
      case ArcManagedOptin::kManagedEnabled:
        test::OobeJS().ExpectDisabledPath(kBackupToggle);
        test::OobeJS().ExpectHasAttribute("checked", kBackupToggle);
        break;
      case ArcManagedOptin::kNotManaged:
        test::OobeJS().ExpectEnabledPath(kBackupToggle);
        break;
    }
    switch (location_opt_in) {
      case ArcManagedOptin::kManagedDisabled:
        test::OobeJS().ExpectDisabledPath(kLocationToggle);
        test::OobeJS().ExpectHasNoAttribute("checked", kLocationToggle);
        break;
      case ArcManagedOptin::kManagedEnabled:
        test::OobeJS().ExpectDisabledPath(kLocationToggle);
        test::OobeJS().ExpectHasAttribute("checked", kLocationToggle);
        break;
      case ArcManagedOptin::kNotManaged:
        test::OobeJS().ExpectEnabledPath(kLocationToggle);
        break;
    }
  }

 private:
  const LoginManagerMixin::TestUserInfo managed_user_{
      AccountId::FromUserEmailGaiaId(kManagedUser, kManagedGaiaID)};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, managed_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenManagedUserTest,
                       BackupOptinManagedEnabled) {
  SetUpArcEnabledPolicy();
  SetUpManagedOptIns(ArcManagedOptin::kManagedEnabled,
                     ArcManagedOptin::kNotManaged);
  LoginManagedUser();
  CheckTogglesState(ArcManagedOptin::kManagedEnabled,
                    ArcManagedOptin::kNotManaged);
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenManagedUserTest,
                       BackupOptinManagedDisabled) {
  SetUpArcEnabledPolicy();
  SetUpManagedOptIns(ArcManagedOptin::kManagedDisabled,
                     ArcManagedOptin::kNotManaged);
  LoginManagedUser();
  CheckTogglesState(ArcManagedOptin::kManagedDisabled,
                    ArcManagedOptin::kNotManaged);
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenManagedUserTest,
                       LocationOptinManagedEnabled) {
  SetUpArcEnabledPolicy();
  SetUpManagedOptIns(ArcManagedOptin::kNotManaged,
                     ArcManagedOptin::kManagedEnabled);
  LoginManagedUser();
  CheckTogglesState(ArcManagedOptin::kNotManaged,
                    ArcManagedOptin::kManagedEnabled);
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenManagedUserTest,
                       LocationOptinManagedDisabled) {
  SetUpArcEnabledPolicy();
  SetUpManagedOptIns(ArcManagedOptin::kNotManaged,
                     ArcManagedOptin::kManagedDisabled);
  LoginManagedUser();
  CheckTogglesState(ArcManagedOptin::kNotManaged,
                    ArcManagedOptin::kManagedDisabled);
}

class ConsolidatedConsentScreenManagedDeviceTest
    : public ConsolidatedConsentScreenTest {
 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// TODO(https://crbug.com/1311968): Update test to test all skipping conditions.
IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenManagedDeviceTest,
                       DISABLED_Skip) {
  LoginAsRegularUser();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            ConsolidatedConsentScreen::Result::NOT_APPLICABLE);
}

// Show the screen in a low resolution to guarantee that the `Read More` button
// is shown.
class ConsolidatedConsentScreenReadMore
    : public ConsolidatedConsentScreenArcEnabledTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Use low resolution screen to force "Read More" button to be shown
    command_line->AppendSwitchASCII("ash-host-window-bounds", "900x600");
    ConsolidatedConsentScreenArcEnabledTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenReadMore, ClickAccept) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();
  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kReadMoreButton) + " != null")
      ->Wait();
  test::OobeJS().ClickOnPath(kReadMoreButton);
  test::OobeJS().CreateVisibilityWaiter(true, kAcceptButton)->Wait();
  test::OobeJS().ClickOnPath(kAcceptButton);
}

// TODO(crbug.com/1298249): Add browsertest to ensure that the metrics consent
// is propagated to the correct preference (ie device/user) depending on the
// type of user.

}  // namespace ash
