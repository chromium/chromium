// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/hash/sha1.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/login/login_pref_names.h"
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
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/proto/cloud_policy.pb.h"
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
const test::UIPath kTermsDescriptionArcEnabled = {kConsolidatedConsentId,
                                                  "termsDescriptionArcEnabled"};
const test::UIPath kTermsDescriptionArcDisabled = {
    kConsolidatedConsentId, "termsDescriptionArcDisabled"};

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

// Umbrella policy states, used for testing multiple policies.
enum class PolicyState {
  kManagedDisabled = 0,
  kManagedEnabled = 1,
  kNotManaged = 2,
};

// See ArcGoogleLocationServicesEnabled.yaml
enum class ArcLocationPolicyValue {
  kDisabled = 0,
  kLetUsersDecide = 1,
  kEnabled = 2,
};

// See DefaultGeolocationSetting.yaml
enum class DefaultGeolocationPolicyValue {
  kAllow = 1,
  kBlock = 2,
  kAsk = 3,
};

// See GoogleLocationServicesEnabled.yaml
enum class CrosLocationPolicyValue {
  kBlock = 0,
  kAllow = 1,
  kOnlyAllowForSystem = 2,
};

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
      IDS_CONSOLIDATED_CONSENT_ARC_LOCATION_OPT_IN_TITLE);
  location_service_consent.add_description_grd_ids(
      IDS_CONSOLIDATED_CONSENT_ARC_LOCATION_OPT_IN);
  location_service_consent.set_status(accepted ? UserConsentTypes::GIVEN
                                               : UserConsentTypes::NOT_GIVEN);
  return location_service_consent;
}

ArcGoogleLocationServiceConsent BuildCrosGoogleLocationServiceConsent(
    bool accepted) {
  ArcGoogleLocationServiceConsent location_service_consent;
  location_service_consent.set_confirmation_grd_id(
      IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
  location_service_consent.add_description_grd_ids(
      IDS_CONSOLIDATED_CONSENT_CROS_LOCATION_OPT_IN_TITLE);
  location_service_consent.add_description_grd_ids(
      IDS_CONSOLIDATED_CONSENT_CROS_LOCATION_OPT_IN);
  location_service_consent.set_status(accepted ? UserConsentTypes::GIVEN
                                               : UserConsentTypes::NOT_GIVEN);
  return location_service_consent;
}
}  // namespace

// Privacy Hub(PH) replaces the ARC location toggle on OOBE with the CrOS
// location toggle. This new toggle will affect all location clients of the
// system, including ARC. This parameter tests both, when PH is enabled and
// disabled.
class PrivacyHubParameterizedTest : public testing::WithParamInterface<bool> {
 public:
  PrivacyHubParameterizedTest() {
    is_ph_enabled_ = GetParam();
    if (is_ph_enabled_) {
      feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
    } else {
      feature_list_.InitAndDisableFeature(ash::features::kCrosPrivacyHub);
    }
  }

  bool IsPhEnabled() { return is_ph_enabled_; }

 private:
  bool is_ph_enabled_ = false;
  base::test::ScopedFeatureList feature_list_;
};

// Regular user flow with ARC not enabled
class ConsolidatedConsentScreenTestBase : public OobeBaseTest {
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

  void SetUpOnMainThread() override {
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    // Override the screen exit callback with our own method.
    ConsolidatedConsentScreen* screen =
        WizardController::default_controller()
            ->GetScreen<ConsolidatedConsentScreen>();

    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());

    OobeBaseTest::SetUpOnMainThread();
  }

  void LoginAsRegularUser() { login_manager_mixin_.LoginAsNewRegularUser(); }

  ConsolidatedConsentScreen::Result WaitForScreenExitResult() {
    auto result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

  std::optional<ConsolidatedConsentScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;

  std::vector<base::Bucket> GetAllRecordedUserActions() {
    return histogram_tester_.GetAllSamples(
        "OOBE.ConsolidatedConsentScreen.UserActions");
  }

 protected:
  base::test::TestFuture<ConsolidatedConsentScreen::Result>
      screen_result_waiter_;
  ConsolidatedConsentScreen::ScreenExitCallback original_callback_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  FakeEulaMixin fake_eula_{&mixin_host_, embedded_test_server()};
};

// Privacy Hub(PH) replaces the ARC location toggle on OOBE with the CrOS
// location toggle. This new toggle will affect all location clients of the
// system, including ARC. This parameter tests both, when PH is enabled and
// disabled.
class ConsolidatedConsentScreenTest : public ConsolidatedConsentScreenTestBase,
                                      public PrivacyHubParameterizedTest {};

// In the legacy code, for regular users with ARC disabled, only usage stats
// opt-in is visible and the toggle is enabled. When PH is enabled, location
// toggle will also be shown.
IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenTest, OptinsVisibility) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ExpectVisiblePath(kTermsDescriptionArcDisabled);
  test::OobeJS().ExpectVisiblePath(kUsageStats);
  test::OobeJS().ExpectEnabledPath(kUsageStatsToggle);
  test::OobeJS().ExpectHiddenPath(kBackup);

  test::OobeJS().ExpectEnabledPath(kRecovery);

  if (IsPhEnabled()) {
    // Location toggle should always be shown when the Privacy Hub location is
    // launched.
    test::OobeJS().ExpectVisiblePath(kLocation);
    test::OobeJS().ExpectVisiblePath(kFooter);
  } else {
    test::OobeJS().ExpectHiddenPath(kLocation);
    test::OobeJS().ExpectHiddenPath(kFooter);
  }
}

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenTest, GoogleEula) {
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

  histogram_tester_.ExpectTotalCount(kGoogleEulaWebviewFirstLoadResult, 1);
  histogram_tester_.ExpectTotalCount(kCrosEulaWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kArcTosWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kPrivacyPolicyFirstLoadResult, 0);
}

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenTest, CrosEula) {
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

  histogram_tester_.ExpectTotalCount(kGoogleEulaWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kCrosEulaWebviewFirstLoadResult, 1);
  histogram_tester_.ExpectTotalCount(kArcTosWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kPrivacyPolicyFirstLoadResult, 0);
}

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenTest, Accept) {
  // The preference `prefs::kOobeStartTime` is not set due to advancing to
  // login.
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetTime(prefs::kOobeStartTime, base::Time::Now());

  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepShownStatus.Consolidated-consent", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepShownStatus2.Consolidated-consent.FirstOnboarding", 1);

  test::OobeJS().CreateVisibilityWaiter(true, kAcceptButton)->Wait();
  test::OobeJS().ClickOnPath(kAcceptButton);
  EXPECT_EQ(WaitForScreenExitResult(),
            ConsolidatedConsentScreen::Result::ACCEPTED);

  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime.Consolidated-consent", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime2.Consolidated-consent.FirstOnboarding", 1);

  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Consolidated-consent."
      "AcceptedRegular",
      1);

  histogram_tester_.ExpectTotalCount(kRecoveryOptInResultHistogram, 1);
  EXPECT_THAT(GetAllRecordedUserActions(),
              ElementsAre(base::Bucket(
                  static_cast<int>(UserAction::kAcceptButtonClicked), 1)));
}

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenTest, LearnMore) {
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

INSTANTIATE_TEST_SUITE_P(EnableDisablePhLocation,
                         ConsolidatedConsentScreenTest,
                         testing::Values(false, true));

class ConsolidatedConsentScreenArcEnabledTestBase
    : public ConsolidatedConsentScreenTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    ConsolidatedConsentScreenTestBase::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    // Enable ARC for testing.
    arc::ArcServiceLauncher::Get()->ResetForTesting();
    ConsolidatedConsentScreenTestBase::SetUpOnMainThread();
  }

  FakeArcTosMixin fake_arc_tos_{&mixin_host_, embedded_test_server()};
};

// Derives from the `PrivacyHubParameterizedTest` to test both scenarios, when
// PH is enabled and disabled.
class ConsolidatedConsentScreenArcEnabledTest
    : public ConsolidatedConsentScreenArcEnabledTestBase,
      public PrivacyHubParameterizedTest {};

// For regular users with ARC enabled, all ARC opt-ins are visible and the
// toggles are enabled. Recovery service availability would depend on feature
// flag.
IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenArcEnabledTest,
                       OptinsVisibility) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ExpectVisiblePath(kTermsDescriptionArcEnabled);
  test::OobeJS().ExpectVisiblePath(kUsageStats);
  test::OobeJS().ExpectEnabledPath(kUsageStatsToggle);
  test::OobeJS().ExpectVisiblePath(kBackup);
  test::OobeJS().ExpectEnabledPath(kBackupToggle);

  test::OobeJS().ExpectEnabledPath(kRecovery);

  test::OobeJS().ExpectVisiblePath(kLocation);
  test::OobeJS().ExpectEnabledPath(kLocationToggle);

  test::OobeJS().ExpectVisiblePath(kFooter);
}

// Make sure that EULA links in the terms description for the ARC Enabled
// shows the correct dialogs.
IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenArcEnabledTest, EULA) {
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

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenArcEnabledTest, ArcToS) {
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
  histogram_tester_.ExpectTotalCount(kGoogleEulaWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kCrosEulaWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kArcTosWebviewFirstLoadResult, 1);
  histogram_tester_.ExpectTotalCount(kPrivacyPolicyFirstLoadResult, 0);
}

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenArcEnabledTest, PrivacyPolicy) {
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
  histogram_tester_.ExpectTotalCount(kGoogleEulaWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kCrosEulaWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kArcTosWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kPrivacyPolicyFirstLoadResult, 1);
}

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenArcEnabledTest,
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

INSTANTIATE_TEST_SUITE_P(All,
                         ConsolidatedConsentScreenArcEnabledTest,
                         testing::Values(false, true));

// There are two toggles for enabling/disabling ARC backup restore and
// ARC location service. This parameterized test executes all combinations
// of enabled/disabled states, both for the legacy(PH=disabled) and
// new(PH=enabled) states and checks advancing to the next screen by accepting.
class ConsolidatedConsentScreenArcEnabledParameterizedTest
    : public ConsolidatedConsentScreenArcEnabledTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  void SetUp() override {
    std::tie(is_ph_enabled_, accept_backup_restore_, accept_location_service_) =
        GetParam();
    if (is_ph_enabled_) {
      feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
    } else {
      feature_list_.InitAndDisableFeature(ash::features::kCrosPrivacyHub);
    }

    ConsolidatedConsentScreenArcEnabledTestBase::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    ConsolidatedConsentScreenArcEnabledTestBase::
        SetUpInProcessBrowserTestFixture();
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ConsolidatedConsentScreenArcEnabledParameterizedTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  bool IsPhEnabled() { return is_ph_enabled_; }

  // Common routine that enables/disables toggles based on test parameters.
  // `play_consent`, `backup_and_restore_consent` and `location_service_consent`
  // are the expected consents recordings.
  void AdvanceNextScreenWithExpectations(
      ArcPlayTermsOfServiceConsent play_consent,
      ArcBackupAndRestoreConsent backup_and_restore_consent,
      ArcGoogleLocationServiceConsent location_service_consent) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    FakeConsentAuditor* auditor = static_cast<FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForProfile(profile));

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

    // PH Location toggle state is not collected through consent auditor.
    if (!is_ph_enabled_) {
      EXPECT_CALL(
          *auditor,
          RecordArcGoogleLocationServiceConsent(
              testing::_, consent_auditor::ArcGoogleLocationServiceConsentEq(
                              location_service_consent)));
    }
    test::OobeJS().CreateVisibilityWaiter(true, kAcceptButton)->Wait();
    test::OobeJS().ClickOnPath(kAcceptButton);
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ConsentAuditorFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildFakeConsentAuditor));
  }

  bool is_ph_enabled_;
  bool accept_backup_restore_;
  bool accept_location_service_;

  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription subscription_;
};

// Tests that clicking on "Accept" button records the expected consents.
// When TOS are accepted we should also record whether backup restores and
// location services are enabled.
IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenArcEnabledParameterizedTest,
                       ClickAccept) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  // Click ARC ToS link to get the ARC ToS loaded and recorded.
  test::OobeJS().ClickOnPath(kArcTosLink);
  test::OobeJS().CreateVisibilityWaiter(true, kArcTosDialog)->Wait();
  test::OobeJS().ClickOnPath(kArcTosOkButton);

  ArcPlayTermsOfServiceConsent play_consent =
      BuildArcPlayTermsOfServiceConsent(fake_arc_tos_.GetArcTosContent());
  ArcBackupAndRestoreConsent backup_and_restore_consent =
      BuildArcBackupAndRestoreConsent(accept_backup_restore_);
  ArcGoogleLocationServiceConsent location_service_consent =
      IsPhEnabled()
          ? BuildCrosGoogleLocationServiceConsent(accept_location_service_)
          : BuildArcGoogleLocationServiceConsent(accept_location_service_);

  AdvanceNextScreenWithExpectations(play_consent, backup_and_restore_consent,
                                    location_service_consent);

  EXPECT_EQ(WaitForScreenExitResult(),
            ConsolidatedConsentScreen::Result::ACCEPTED);

  // Only ARC ToS is loaded.
  histogram_tester_.ExpectTotalCount(kGoogleEulaWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kCrosEulaWebviewFirstLoadResult, 0);
  histogram_tester_.ExpectTotalCount(kArcTosWebviewFirstLoadResult, 1);
  histogram_tester_.ExpectTotalCount(kPrivacyPolicyFirstLoadResult, 0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ConsolidatedConsentScreenArcEnabledParameterizedTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

class ConsolidatedConsentScreenManagedUserTest
    : public ConsolidatedConsentScreenArcEnabledTest {
 public:
  enum class ArcManagedOptin {
    kManagedDisabled = 0,
    kNotManaged = 1,
    kManagedEnabled = 2,
  };

  // See `GoogleLocationServicesEnabled.yaml` file.
  enum class CrosLocationManagedOptin {
    kManagedDisabled = 0,
    kManagedEnabled = 1,
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

  void SetUpManagedArcOptIns(ArcManagedOptin backup_opt_in,
                             ArcManagedOptin location_opt_in) {
    std::unique_ptr<ScopedUserPolicyUpdate> scoped_user_policy_update =
        user_policy_mixin_.RequestPolicyUpdate();
    scoped_user_policy_update->policy_payload()
        ->mutable_arcbackuprestoreserviceenabled()
        ->set_value(int(backup_opt_in));

    if (IsPhEnabled()) {
      // Map the ARC location optin intent to CrOS.
      switch (location_opt_in) {
        case (ArcManagedOptin::kManagedDisabled):
          scoped_user_policy_update->policy_payload()
              ->mutable_subproto1()
              ->mutable_googlelocationservicesenabled()
              ->set_value(
                  static_cast<int>(CrosLocationManagedOptin::kManagedDisabled));
          break;
        case (ArcManagedOptin::kManagedEnabled):
          scoped_user_policy_update->policy_payload()
              ->mutable_subproto1()
              ->mutable_googlelocationservicesenabled()
              ->set_value(
                  static_cast<int>(CrosLocationManagedOptin::kManagedEnabled));
          break;
        case (ArcManagedOptin::kNotManaged):
          // "Let users decide" has is an explicit value for
          // `ArcGoogleLocationServicesEnabled` policy, while the
          // `GoogleLocationServicesEnabled` equivalent is the unset policy
          // value.
          scoped_user_policy_update->policy_payload()
              ->mutable_subproto1()
              ->clear_googlelocationservicesenabled();
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    } else {
      // Legacy handling.
      scoped_user_policy_update->policy_payload()
          ->mutable_arcgooglelocationservicesenabled()
          ->set_value(int(location_opt_in));
    }
  }

  void CheckTogglesState(ArcManagedOptin backup_opt_in,
                         ArcManagedOptin location_opt_in) {
    // Legacy handling.
    // `IsPhEnabled() == true` is granularly tested below.
    if (!IsPhEnabled()) {
      test::OobeJS().ExpectVisiblePath(kBackup);
      test::OobeJS().ExpectVisiblePath(kLocation);
    }

    switch (backup_opt_in) {
      case ArcManagedOptin::kManagedDisabled:
        if (IsPhEnabled()) {
          test::OobeJS().ExpectHiddenPath(kBackup);
        } else {
          test::OobeJS().ExpectDisabledPath(kBackupToggle);
          test::OobeJS().ExpectHasNoAttribute("checked", kBackupToggle);
        }
        break;
      case ArcManagedOptin::kManagedEnabled:
        if (IsPhEnabled()) {
          test::OobeJS().ExpectHiddenPath(kBackup);
        } else {
          test::OobeJS().ExpectDisabledPath(kBackupToggle);
          test::OobeJS().ExpectHasAttribute("checked", kBackupToggle);
        }
        break;
      case ArcManagedOptin::kNotManaged:
        if (IsPhEnabled()) {
          test::OobeJS().ExpectVisiblePath(kBackup);
        }
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

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenManagedUserTest,
                       BackupOptinManagedEnabled) {
  SetUpArcEnabledPolicy();
  SetUpManagedArcOptIns(ArcManagedOptin::kManagedEnabled,
                        ArcManagedOptin::kNotManaged);
  LoginManagedUser();
  CheckTogglesState(ArcManagedOptin::kManagedEnabled,
                    ArcManagedOptin::kNotManaged);
}

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenManagedUserTest,
                       BackupOptinManagedDisabled) {
  SetUpArcEnabledPolicy();
  SetUpManagedArcOptIns(ArcManagedOptin::kManagedDisabled,
                        ArcManagedOptin::kNotManaged);
  LoginManagedUser();
  CheckTogglesState(ArcManagedOptin::kManagedDisabled,
                    ArcManagedOptin::kNotManaged);
}

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenManagedUserTest,
                       LocationOptinManagedEnabled) {
  SetUpArcEnabledPolicy();
  SetUpManagedArcOptIns(ArcManagedOptin::kNotManaged,
                        ArcManagedOptin::kManagedEnabled);
  LoginManagedUser();
  CheckTogglesState(ArcManagedOptin::kNotManaged,
                    ArcManagedOptin::kManagedEnabled);
}

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenManagedUserTest,
                       LocationOptinManagedDisabled) {
  SetUpArcEnabledPolicy();
  SetUpManagedArcOptIns(ArcManagedOptin::kNotManaged,
                        ArcManagedOptin::kManagedDisabled);
  LoginManagedUser();
  CheckTogglesState(ArcManagedOptin::kNotManaged,
                    ArcManagedOptin::kManagedDisabled);
}

INSTANTIATE_TEST_SUITE_P(EnableDisablePhLocation,
                         ConsolidatedConsentScreenManagedUserTest,
                         testing::Values(false, true));

// Privacy Hub Location is enabled. Legacy handling is tested above.
class ConsolidatedConsentScreenManagedUserLocationPolicyInterplayTest
    : public ConsolidatedConsentScreenArcEnabledTestBase,
      public testing::WithParamInterface<
          std::tuple<PolicyState, PolicyState, PolicyState>> {
 public:
  ConsolidatedConsentScreenManagedUserLocationPolicyInterplayTest() {
    std::tie(arc_geo_policy_value_, default_geo_policy_value_,
             cros_geo_policy_value_) = GetParam();

    feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
  }

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

  void SetArcLocationPolicy(PolicyState value) {
    std::unique_ptr<ScopedUserPolicyUpdate> scoped_user_policy_update =
        user_policy_mixin_.RequestPolicyUpdate();

    int policy_value;
    switch (value) {
      case (PolicyState::kNotManaged):
        policy_value =
            static_cast<int>(ArcLocationPolicyValue::kLetUsersDecide);
        break;
      case (PolicyState::kManagedDisabled):
        policy_value = static_cast<int>(ArcLocationPolicyValue::kDisabled);
        break;
      case (PolicyState::kManagedEnabled):
        policy_value = static_cast<int>(ArcLocationPolicyValue::kEnabled);
        break;
    }

    scoped_user_policy_update->policy_payload()
        ->mutable_arcgooglelocationservicesenabled()
        ->set_value(policy_value);
  }

  void SetDefaultGeolocationPolicy(PolicyState value) {
    std::unique_ptr<ScopedUserPolicyUpdate> scoped_user_policy_update =
        user_policy_mixin_.RequestPolicyUpdate();

    int policy_value;
    switch (value) {
      case (PolicyState::kNotManaged):
        scoped_user_policy_update->policy_payload()
            ->clear_defaultgeolocationsetting();
        return;
      case (PolicyState::kManagedDisabled):
        policy_value = static_cast<int>(DefaultGeolocationPolicyValue::kBlock);
        break;
      case (PolicyState::kManagedEnabled):
        policy_value = static_cast<int>(DefaultGeolocationPolicyValue::kAllow);
        break;
    }
    scoped_user_policy_update->policy_payload()
        ->mutable_defaultgeolocationsetting()
        ->set_value(policy_value);
  }

  void SetCrosLocationPolicy(PolicyState value) {
    std::unique_ptr<ScopedUserPolicyUpdate> scoped_user_policy_update =
        user_policy_mixin_.RequestPolicyUpdate();

    int policy_value;
    switch (value) {
      case (PolicyState::kNotManaged):
        scoped_user_policy_update->policy_payload()
            ->mutable_subproto1()
            ->clear_googlelocationservicesenabled();
        return;
      case (PolicyState::kManagedDisabled):
        policy_value = static_cast<int>(CrosLocationPolicyValue::kBlock);
        break;
      case (PolicyState::kManagedEnabled):
        policy_value = static_cast<int>(CrosLocationPolicyValue::kAllow);
        break;
    }

    scoped_user_policy_update->policy_payload()
        ->mutable_subproto1()
        ->mutable_googlelocationservicesenabled()
        ->set_value(policy_value);
  }

  void CheckTogglesState(PolicyState arc_geo_policy_value,
                         PolicyState default_geo_policy_value,
                         PolicyState cros_geo_policy_value) {
    CHECK(ash::features::IsCrosPrivacyHubLocationEnabled());

    // Location toggle is always shown when PH Location is enabled.
    test::OobeJS().ExpectVisiblePath(kLocation);

    // `cros_geo_policy_value` takes the precedence over the other policies.
    switch (cros_geo_policy_value) {
      case PolicyState::kManagedDisabled:
        test::OobeJS().ExpectDisabledPath(kLocationToggle);
        test::OobeJS().ExpectHasNoAttribute("checked", kLocationToggle);
        return;
      case PolicyState::kManagedEnabled:
        test::OobeJS().ExpectDisabledPath(kLocationToggle);
        test::OobeJS().ExpectHasAttribute("checked", kLocationToggle);
        return;
      case PolicyState::kNotManaged:
        // Depends on the other policies, processed below.
        break;
    }

    // `arc_geo_policy_value` no longer affects the OOBE location toggle.
    // `default_geo_policy_value` only has effect when set to
    // `kManagedDisabled`.
    if (cros_geo_policy_value == PolicyState::kNotManaged) {
      if (default_geo_policy_value == PolicyState::kManagedDisabled) {
        test::OobeJS().ExpectDisabledPath(kLocationToggle);
        test::OobeJS().ExpectHasNoAttribute("checked", kLocationToggle);
        return;
      }
    }

    test::OobeJS().ExpectEnabledPath(kLocationToggle);
  }

 protected:
  PolicyState arc_geo_policy_value_ = PolicyState::kNotManaged;
  PolicyState default_geo_policy_value_ = PolicyState::kNotManaged;
  PolicyState cros_geo_policy_value_ = PolicyState::kNotManaged;

 private:
  const LoginManagerMixin::TestUserInfo managed_user_{
      AccountId::FromUserEmailGaiaId(kManagedUser, kManagedGaiaID)};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, managed_user_.account_id};

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    ConsolidatedConsentScreenManagedUserLocationPolicyInterplayTest,
    AllPolicyVariations) {
  SetUpArcEnabledPolicy();
  SetArcLocationPolicy(arc_geo_policy_value_);
  SetDefaultGeolocationPolicy(default_geo_policy_value_);
  SetCrosLocationPolicy(cros_geo_policy_value_);

  LoginManagedUser();
  CheckTogglesState(arc_geo_policy_value_, default_geo_policy_value_,
                    cros_geo_policy_value_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ConsolidatedConsentScreenManagedUserLocationPolicyInterplayTest,
    testing::Combine(
        /*ArcGoogleLocationServicesEnabled=*/
        testing::Values(PolicyState::kManagedDisabled,
                        PolicyState::kManagedEnabled,
                        PolicyState::kNotManaged),
        /*DefaultGeolocationSetting=*/
        testing::Values(PolicyState::kManagedDisabled,
                        PolicyState::kManagedEnabled,
                        PolicyState::kNotManaged),
        /*GoogleLocationServicesEnabled=*/
        testing::Values(PolicyState::kManagedDisabled,
                        PolicyState::kManagedEnabled,
                        PolicyState::kNotManaged)));

class ConsolidatedConsentScreenManagedDeviceTest
    : public ConsolidatedConsentScreenTest {
 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// TODO(https://crbug.com/1311968): Update test to test all skipping conditions.
IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenManagedDeviceTest,
                       DISABLED_Skip) {
  LoginAsRegularUser();
  EXPECT_EQ(WaitForScreenExitResult(),
            ConsolidatedConsentScreen::Result::NOT_APPLICABLE);
}

INSTANTIATE_TEST_SUITE_P(EnableDisablePhLocation,
                         ConsolidatedConsentScreenManagedDeviceTest,
                         testing::Values(/*PhEnabled=*/false,
                                         /*PhEnabled=*/true));

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

IN_PROC_BROWSER_TEST_P(ConsolidatedConsentScreenReadMore, ClickAccept) {
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

INSTANTIATE_TEST_SUITE_P(EnableDisablePhLocation,
                         ConsolidatedConsentScreenReadMore,
                         testing::Values(false, true));

// TODO(crbug.com/1298249): Add browsertest to ensure that the metrics consent
// is propagated to the correct preference (ie device/user) depending on the
// type of user.

}  // namespace ash
