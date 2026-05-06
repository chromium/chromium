// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#endif

namespace glic {
namespace {

class GlicMetricsBrowserTest : public InProcessBrowserTest {
 public:
  GlicMetricsBrowserTest() : GlicMetricsBrowserTest({}, {}) {}

 protected:
  explicit GlicMetricsBrowserTest(
      const std::vector<base::test::FeatureRef>& extra_enabled_features,
      const std::vector<base::test::FeatureRef>& extra_disabled_features = {}) {
    std::vector<base::test::FeatureRef> enabled_features =
        GetDefaultEnabledGlicTestFeatures();

    enabled_features.insert(enabled_features.end(),
                            extra_enabled_features.begin(),
                            extra_enabled_features.end());

    glic_test_environment_ = std::make_unique<GlicTestEnvironment>(
        GlicTestEnvironmentConfig{.fre_status = std::nullopt}, enabled_features,
        extra_disabled_features);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetFRECompletion(browser()->profile(), prefs::FreStatus::kNotStarted);
  }

  std::unique_ptr<GlicTestEnvironment> glic_test_environment_;
};

class GlicMetricsBrowserTestWithMessageFirstFre
    : public GlicMetricsBrowserTest {
 public:
  GlicMetricsBrowserTestWithMessageFirstFre()
      : GlicMetricsBrowserTest({features::kGlicMessageFirstFre}, {}) {}
};

class GlicMetricsBrowserTestWithMessageFirstFreDisabled
    : public GlicMetricsBrowserTest {
 public:
  GlicMetricsBrowserTestWithMessageFirstFreDisabled()
      : GlicMetricsBrowserTest({}, {features::kGlicMessageFirstFre}) {}
};

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTestWithMessageFirstFre,
                       GlicFreShown_MessageFirstFreEnabled) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  SetFRECompletion(browser()->profile(), prefs::FreStatus::kNotStarted);

  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Shown"), 1);

  histogram_tester.ExpectUniqueSample("Glic.Fre.Shown.InvocationSource",
                                      mojom::InvocationSource::kOsButton, 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest, GlicFreShown_MultiInstance) {
  base::UserActionTester user_action_tester;

  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Shown"), 1);

  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Dismissed.Onboarding"),
            1);
}

// Test with message first FRE disabled.
// Expected behavior: Normal toggle flow. Logs ToggleSource on both open and
// close.
IN_PROC_BROWSER_TEST_F(
    GlicMetricsBrowserTestWithMessageFirstFreDisabled,
    ToggleAndOpenSourceMetrics_SidePanel_MessageFirstFreDisabled) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Open the side panel
  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.ToggleSource",
                                      mojom::InvocationSource::kOsButton, 1);
  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Open"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Toggle"), 1);

  // Close the side panel
  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.ToggleSource",
                                      mojom::InvocationSource::kOsButton, 2);
  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Close"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Toggle"), 2);
}

// Test with message first FRE enabled.
// Expected behavior:
// 1. The first call to ToggleUI() (to open the panel) is intercepted because
//    the user hasn't completed FRE. It redirects to the Invoke flow, which
//    logs OpenSource but NOT ToggleSource.
// 2. The second call to ToggleUI() (to close the panel) proceeds normally
//    because the panel is already open. This logs ToggleSource as expected.
IN_PROC_BROWSER_TEST_F(
    GlicMetricsBrowserTestWithMessageFirstFre,
    ToggleAndOpenSourceMetrics_SidePanel_MessageFirstFreEnabled) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Open the side panel. Since FRE is not completed and GlicMessageFirstFre is
  // enabled, this calls Invoke instead of normal Toggle.
  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  // ToggleSource is NOT logged for the first call because it went through
  // Invoke.
  histogram_tester.ExpectTotalCount("Glic.Instance.SidePanel.ToggleSource", 0);
  // Toggle action is also not logged.
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Toggle"), 0);

  // OpenSource and Open action ARE logged by Invoke.
  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Open"), 1);

  // Close the side panel. Now that the panel is open, MaybeInvoke returns
  // false, and it proceeds to normal Toggle flow to close it.
  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  // Now ToggleSource SHOULD be logged (1 sample).
  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.ToggleSource",
                                      mojom::InvocationSource::kOsButton, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Toggle"), 1);

  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Close"), 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       InvokeAndOpenSourceMetrics_SidePanel) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  GlicInvokeOptions options(mojom::InvocationSource::kNavigationCapture);
  options.target.conversation = DefaultConversation{};
  options.target.surface = TabListInterface::From(browser())->GetActiveTab();

  glic_service->Invoke(std::move(options));

  // Verify that GlicInstanceMetrics::OnOpen was called with kNavigationCapture.
  histogram_tester.ExpectUniqueSample(
      "Glic.Instance.SidePanel.OpenSource",
      mojom::InvocationSource::kNavigationCapture, 1);

  // Verify metrics logged in OnOpen.
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Open"), 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Instance.InitialInvocationSource",
      mojom::InvocationSource::kNavigationCapture, 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       Invoke_NewConversationMetrics_SidePanel) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // 1. Open the side panel first via ToggleUI.
  glic_service->ToggleUI(browser(), /*prevent_close=*/false,
                         mojom::InvocationSource::kOsButton);

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Open"), 1);
  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);

  // 2. Call Invoke with a NEW conversation.
  GlicInvokeOptions options(mojom::InvocationSource::kNavigationCapture);
  options.target.conversation = NewConversation{};
  options.target.surface = TabListInterface::From(browser())->GetActiveTab();

  base::HistogramTester histogram_tester_invoke;
  glic_service->Invoke(std::move(options));

  // 3. Verify that Glic.Instance.Open IS incremented because a new instance is
  // created.
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Open"), 2);

  // 4. Verify that OpenSource metric was logged for kNavigationCapture.
  histogram_tester_invoke.ExpectUniqueSample(
      "Glic.Instance.SidePanel.OpenSource",
      mojom::InvocationSource::kNavigationCapture, 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       Invoke_CurrentConversation_SidePanel) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // 1. Open the side panel first via ToggleUI.
  glic_service->ToggleUI(browser(), /*prevent_close=*/false,
                         mojom::InvocationSource::kOsButton);

  auto* tab = TabListInterface::From(browser())->GetActiveTab();
  auto* coordinator = GlicSidePanelCoordinator::GetForTab(tab);
  ASSERT_TRUE(coordinator);
  ASSERT_TRUE(base::test::RunUntil([&]() { return coordinator->IsShowing(); }));

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Open"), 1);
  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);

  // 2. Call Invoke with DefaultConversation (representing current
  // conversation).
  GlicInvokeOptions options(mojom::InvocationSource::kNavigationCapture);
  options.target.conversation = DefaultConversation{};
  options.target.surface = TabListInterface::From(browser())->GetActiveTab();

  glic_service->Invoke(std::move(options));

  // 3. Verify that Glic.Instance.Open is NOT incremented.
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Open"), 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       ToggleAndOpenSourceMetrics_Floaty) {
  base::HistogramTester histogram_tester;

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // First toggle the UI to create the floaty instance.
  glic_service->instance_coordinator().Toggle(
      /*browser=*/nullptr, /*prevent_close=*/false,
      mojom::InvocationSource::kOsHotkey,
      /*deprecated_prompt_suggestion=*/std::nullopt);

  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.ToggleSource",
                                      mojom::InvocationSource::kOsHotkey, 1);
  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.OpenSource",
                                      mojom::InvocationSource::kOsHotkey, 1);

  // Close the floaty panel.
  glic_service->instance_coordinator().Toggle(
      /*browser=*/nullptr, /*prevent_close=*/false,
      mojom::InvocationSource::kOsHotkey,
      /*deprecated_prompt_suggestion=*/std::nullopt);

  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.ToggleSource",
                                      mojom::InvocationSource::kOsHotkey, 2);
  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.OpenSource",
                                      mojom::InvocationSource::kOsHotkey, 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest, ZoomLevel_OnOpen) {
  base::HistogramTester histogram_tester;

  // Set zoom level for profile.
  browser()->profile()->GetPrefs()->SetInteger(prefs::kGlicZoomLevel, 150);

  // Open the side panel.
  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  // Verify that Glic.ZoomLevel.OnOpen was logged with the correct value.
  histogram_tester.ExpectUniqueSample("Glic.ZoomLevel.OnOpen", 150, 1);
}

class GlicMetricsBrowserTestWithCaptureRegion : public GlicMetricsBrowserTest {
 public:
  GlicMetricsBrowserTestWithCaptureRegion()
      : GlicMetricsBrowserTest(
            /*extra_enabled_features=*/{features::kGlicCaptureRegion},
            /*extra_disabled_features=*/
            {}) {}
};

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/500964398): This test is flaky.
IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTestWithCaptureRegion,
                       DISABLED_SelectionUsedFromController) {
  // The feature is enabled in constructor of
  // GlicMetricsBrowserTestWithCaptureRegion but let's double check.
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kGlicCaptureRegion));

  base::HistogramTester histogram_tester;
  // Open the side panel
  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Simulate showing the overlay.
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  controller->Show(/*options=*/nullptr);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return controller->state() == SelectionOverlayController::State::kOverlay;
  }));
  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->AdjustRegion(selection::SelectedRegion::New(
          base::UnguessableToken::Create(),
          selection::RegionShape::NewRect(gfx::RectF(10, 10, 10, 10))));

  auto* tab_interface = tabs::TabInterface::GetFromContents(web_contents);
  glic::GlicInstanceTracker tracker(browser()->profile());
  tracker.TrackGlicInstanceWithTabHandle(tab_interface->GetHandle());
  auto* host = tracker.GetHost();
  CHECK(host);

  host->instance_metrics_backwards_compatibility().OnUserInputSubmitted(
      mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 1, 1);

  // Submit another input, should still log 1.
  host->instance_metrics_backwards_compatibility().OnUserInputSubmitted(
      mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 1, 2);

  // Close the overlay.
  SelectionOverlayController::FromTabWebContents(web_contents)->Close();

  // Submit another input, should log 0.
  host->instance_metrics_backwards_compatibility().OnUserInputSubmitted(
      mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 3);

  // Close the side panel
  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);
}
#endif

}  // namespace
}  // namespace glic
