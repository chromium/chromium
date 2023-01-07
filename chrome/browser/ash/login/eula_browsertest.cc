// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/test/dialog_window_waiter.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/scoped_help_app_for_test.h"
#include "chrome/browser/ash/login/test/webview_content_extractor.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_display_host_common.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::ElementsAre;

const test::UIPath kEulaWebview = {"oobe-eula-md", "crosEulaFrame"};
const test::UIPath kEulaDialog = {"oobe-eula-md", "eulaDialog"};
const test::UIPath kAcceptEulaButton = {"oobe-eula-md", "acceptButton"};
const test::UIPath kUsageStats = {"oobe-eula-md", "usageStats"};
const test::UIPath kAdditionalTermsLink = {"oobe-eula-md", "additionalTerms"};
const test::UIPath kAdditionalTermsDialog = {"oobe-eula-md", "additionalToS"};
const test::UIPath kLearnMoreLink = {"oobe-eula-md", "learnMore"};
const test::UIPath kBackButton = {"oobe-eula-md", "backButton"};

const char kRemoraRequisition[] = "remora";

class EulaTest : public OobeBaseTest {
 public:
  EulaTest() {
    // EULA screen is not shown when OobeConsolidatedConsent is enabled, and
    // its content is moved to the consolidated consent screen.
    feature_list_.InitAndDisableFeature(features::kOobeConsolidatedConsent);
  }

  EulaTest(const EulaTest&) = delete;
  EulaTest& operator=(const EulaTest&) = delete;

  ~EulaTest() override = default;

  void ShowEulaScreen() {
    LoginDisplayHost::default_host()->StartWizard(EulaView::kScreenId);
    OobeScreenWaiter(EulaView::kScreenId).Wait();
    // Wait until the webview has finished loading.
    test::OobeJS().CreateVisibilityWaiter(true, kEulaDialog)->Wait();
  }

 protected:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
  }

  base::OnceClosure SetCollectStatsConsentClosure(bool consented) {
    return base::BindOnce(
        base::IgnoreResult(&GoogleUpdateSettings::SetCollectStatsConsent),
        consented);
  }

  // Calls `GoogleUpdateSettings::SetCollectStatsConsent` asynchronously on its
  // task runner. Blocks until task is executed.
  void SetGoogleCollectStatsConsent(bool consented) {
    base::RunLoop runloop;
    GoogleUpdateSettings::CollectStatsConsentTaskRunner()->PostTaskAndReply(
        FROM_HERE, SetCollectStatsConsentClosure(consented),
        runloop.QuitClosure());
    runloop.Run();
  }

  // Calls `GoogleUpdateSettings::GetCollectStatsConsent` asynchronously on its
  // task runner. Blocks until task is executed and returns the result.
  bool GetGoogleCollectStatsConsent() {
    bool consented = false;

    // Callback runs after GetCollectStatsConsent is executed. Sets the local
    // variable `consented` to the result of GetCollectStatsConsent.
    auto on_get_collect_stats_consent_callback =
        [](base::OnceClosure quit_closure, bool* consented_out,
           bool consented_result) {
          *consented_out = consented_result;
          std::move(quit_closure).Run();
        };

    base::RunLoop runloop;
    GoogleUpdateSettings::CollectStatsConsentTaskRunner()
        ->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&GoogleUpdateSettings::GetCollectStatsConsent),
            base::BindOnce(on_get_collect_stats_consent_callback,
                           runloop.QuitClosure(), &consented));
    runloop.Run();

    return consented;
  }

  base::test::ScopedFeatureList feature_list_;
  FakeEulaMixin fake_eula_{&mixin_host_, embedded_test_server()};
};

// When testing the offline fallback mechanism, the requests reaching the
// embedded server have to be handled differently.
class EulaOfflineTest : public EulaTest {
 public:
  EulaOfflineTest() { fake_eula_.set_force_http_unavailable(true); }

  ~EulaOfflineTest() override = default;
};

// Tests that offline version is shown when the online version is not
// accessible.
IN_PROC_BROWSER_TEST_F(EulaOfflineTest, LoadOffline) {
  ShowEulaScreen();

  EXPECT_TRUE(test::GetWebViewContents(kEulaWebview)
                  .find(FakeEulaMixin::kOfflineEULAWarning) !=
              std::string::npos);
}

// Tests that online version is shown when it is accessible.
IN_PROC_BROWSER_TEST_F(EulaTest, LoadOnline) {
  ShowEulaScreen();

  const std::string webview_contents = test::GetWebViewContents(kEulaWebview);
  EXPECT_TRUE(webview_contents.find(FakeEulaMixin::kFakeOnlineEula) !=
              std::string::npos);
}

// Verifies statistic collection accepted flow.
// Advaces to the next screen and verifies stats collection is enabled.
IN_PROC_BROWSER_TEST_F(EulaTest, EnableUsageStats) {
  base::HistogramTester histogram_tester;
  ShowEulaScreen();

  // Verify that toggle is enabled by default.
  test::OobeJS().ExpectAttributeEQ("checked", kUsageStats, true);

  ASSERT_TRUE(StatsReportingController::IsInitialized());

  // Explicitly set as false to make sure test modifies these values.
  StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), false);
  g_browser_process->local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, false);
  SetGoogleCollectStatsConsent(false);

  // Start Listening for StatsReportingController updates.
  base::RunLoop runloop;
  auto subscription =
      StatsReportingController::Get()->AddObserver(runloop.QuitClosure());

  // Enable and disable usageStats that to see that metrics are recorded.
  test::OobeJS().TapOnPath(kUsageStats);
  test::OobeJS().TapOnPath(kUsageStats);
  // Advance to the next screen for changes to take effect.
  test::OobeJS().TapOnPath(kAcceptEulaButton);

  // Wait for StartReporting update.
  runloop.Run();

  // Verify stats collection is enabled.
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled));
  EXPECT_TRUE(GetGoogleCollectStatsConsent());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("OOBE.EulaScreen.UserActions"),
      ElementsAre(
          base::Bucket(
              static_cast<int>(EulaScreen::UserAction::kAcceptButtonClicked),
              1),
          base::Bucket(
              static_cast<int>(EulaScreen::UserAction::kUnselectStatsUsage), 1),
          base::Bucket(
              static_cast<int>(EulaScreen::UserAction::kSelectStatsUsage), 1)));
}

// Verify statistic collection denied flow. Clicks on usage stats toggle,
// advaces to the next screen and verifies stats collection is disabled.
IN_PROC_BROWSER_TEST_F(EulaTest, DisableUsageStats) {
  base::HistogramTester histogram_tester;
  ShowEulaScreen();

  // Verify that toggle is enabled by default.
  test::OobeJS().ExpectAttributeEQ("checked", kUsageStats, true);

  ASSERT_TRUE(StatsReportingController::IsInitialized());

  // Explicitly set as true to make sure test modifies these values.
  StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), true);
  g_browser_process->local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, true);
  SetGoogleCollectStatsConsent(true);

  // Start Listening for StatsReportingController updates.
  base::RunLoop runloop;
  auto subscription =
      StatsReportingController::Get()->AddObserver(runloop.QuitClosure());

  // Click on the toggle to disable stats collection and advance to the next
  // screen for changes to take effect.
  test::OobeJS().TapOnPath(kUsageStats);
  test::OobeJS().TapOnPath(kAcceptEulaButton);

  // Wait for StartReportingController update.
  runloop.Run();

  // Verify stats collection is disabled.
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(g_browser_process->local_state()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled));
  EXPECT_FALSE(GetGoogleCollectStatsConsent());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("OOBE.EulaScreen.UserActions"),
      ElementsAre(
          base::Bucket(
              static_cast<int>(EulaScreen::UserAction::kAcceptButtonClicked),
              1),
          base::Bucket(
              static_cast<int>(EulaScreen::UserAction::kUnselectStatsUsage),
              1)));
}

// Tests that clicking on "Learn more" button opens a help dialog.
IN_PROC_BROWSER_TEST_F(EulaTest, LearnMore) {
  base::HistogramTester histogram_tester;
  ShowEulaScreen();

  // Load HelperApp extension.
  ScopedHelpAppForTest scoped_help_app;

  // Start listening for help dialog creation.
  DialogWindowWaiter waiter(
      l10n_util::GetStringUTF16(IDS_LOGIN_OOBE_HELP_DIALOG_TITLE));

  test::OobeJS().TapLinkOnPath(kLearnMoreLink);

  // Wait until help dialog is displayed.
  waiter.Wait();
  EXPECT_THAT(
      histogram_tester.GetAllSamples("OOBE.EulaScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(EulaScreen::UserAction::kShowStatsUsageLearnMore),
          1)));
}

#if defined(NDEBUG)
#define MAYBE_AdditionalToS DISABLED_AdditionalToS
#else
#define MAYBE_AdditionalToS AdditionalToS
#endif
// Tests that "Additional ToS" dialog could be opened and closed.
IN_PROC_BROWSER_TEST_F(EulaTest, MAYBE_AdditionalToS) {
  base::HistogramTester histogram_tester;
  ShowEulaScreen();

  test::OobeJS().TapLinkOnPath(kAdditionalTermsLink);

  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kAdditionalTermsDialog) + ".open")
      ->Wait();

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_RETURN, false /* control */, false /* shift */,
      false /* alt */, false /* command */));

  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kAdditionalTermsDialog) +
                    ".open === false")
      ->Wait();
  test::OobeJS().ExpectFocused(kAdditionalTermsLink);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("OOBE.EulaScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(EulaScreen::UserAction::kShowAdditionalTos), 1)));
}

// Skipped EULA for Remora Requisition.
IN_PROC_BROWSER_TEST_F(EulaTest, SkippedEula) {
  ASSERT_TRUE(StatsReportingController::IsInitialized());

  // Explicitly set as true to make sure test modifies these values.
  StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), true);
  g_browser_process->local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, true);
  SetGoogleCollectStatsConsent(true);

  // Start Listening for StatsReportingController updates.
  base::RunLoop runloop;
  auto subscription =
      StatsReportingController::Get()->AddObserver(runloop.QuitClosure());

  policy::EnrollmentRequisitionManager::SetDeviceRequisition(
      kRemoraRequisition);
  LoginDisplayHost::default_host()->StartWizard(EulaView::kScreenId);

  // Wait for StartReportingController update.
  runloop.Run();

  // Verify stats collection is disabled.
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(g_browser_process->local_state()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled));
  EXPECT_FALSE(GetGoogleCollectStatsConsent());
}

IN_PROC_BROWSER_TEST_F(EulaTest, ClickBack) {
  ShowEulaScreen();
  test::OobeJS().ClickOnPath(kBackButton);
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

}  // namespace
}  // namespace ash
