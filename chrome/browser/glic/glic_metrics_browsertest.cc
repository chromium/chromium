// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
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
    enabled_features.push_back(features::kGlicTrustFirstOnboarding);
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
  ASSERT_TRUE(GlicEnabling::IsMultiInstanceEnabled());

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

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       ToggleAndOpenSourceMetrics_SidePanel) {
  ASSERT_TRUE(GlicEnabling::IsMultiInstanceEnabled());

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

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       InvokeAndOpenSourceMetrics_SidePanel) {
  ASSERT_TRUE(GlicEnabling::IsMultiInstanceEnabled());

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* tab = tabs::TabInterface::GetFromContents(web_contents);
  ASSERT_TRUE(tab);

  GlicInvokeOptions options(mojom::InvocationSource::kNavigationCapture);
  options.conversation = DefaultConversation{};

  glic_service->Invoke(tab, std::move(options));

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
  ASSERT_TRUE(GlicEnabling::IsMultiInstanceEnabled());

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* tab = tabs::TabInterface::GetFromContents(web_contents);
  ASSERT_TRUE(tab);

  // 1. Open the side panel first via ToggleUI.
  glic_service->ToggleUI(browser(), /*prevent_close=*/false,
                         mojom::InvocationSource::kOsButton);

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Open"), 1);
  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);

  // 2. Call Invoke with a NEW conversation.
  GlicInvokeOptions options(mojom::InvocationSource::kNavigationCapture);
  options.conversation = NewConversation{};

  base::HistogramTester histogram_tester_invoke;
  glic_service->Invoke(tab, std::move(options));

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
  ASSERT_TRUE(GlicEnabling::IsMultiInstanceEnabled());

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* tab = tabs::TabInterface::GetFromContents(web_contents);
  ASSERT_TRUE(tab);

  // 1. Open the side panel first via ToggleUI.
  glic_service->ToggleUI(browser(), /*prevent_close=*/false,
                         mojom::InvocationSource::kOsButton);

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Open"), 1);
  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);

  // 2. Call Invoke with DefaultConversation (representing current
  // conversation).
  GlicInvokeOptions options(mojom::InvocationSource::kNavigationCapture);
  options.conversation = DefaultConversation{};

  glic_service->Invoke(tab, std::move(options));

  // 3. Verify that Glic.Instance.Open is NOT incremented.
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Instance.Open"), 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       ToggleAndOpenSourceMetrics_Floaty) {
  ASSERT_TRUE(GlicEnabling::IsMultiInstanceEnabled());

  base::HistogramTester histogram_tester;

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // First toggle the UI to create the floaty instance.
  glic_service->instance_coordinator().Toggle(
      /*browser=*/nullptr, /*prevent_close=*/false,
      mojom::InvocationSource::kOsHotkey, std::nullopt, false, std::nullopt);

  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.ToggleSource",
                                      mojom::InvocationSource::kOsHotkey, 1);
  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.OpenSource",
                                      mojom::InvocationSource::kOsHotkey, 1);

  // Close the floaty panel.
  glic_service->instance_coordinator().Toggle(
      /*browser=*/nullptr, /*prevent_close=*/false,
      mojom::InvocationSource::kOsHotkey, std::nullopt, false, std::nullopt);

  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.ToggleSource",
                                      mojom::InvocationSource::kOsHotkey, 2);
  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.OpenSource",
                                      mojom::InvocationSource::kOsHotkey, 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest, PercentOverlapRounding) {
  base::HistogramTester histogram_tester;
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // Ensure browser is visible for the IsBrowserVisible() check.
  browser()->window()->Show();

  gfx::Rect browser_bounds = BrowserView::GetBrowserViewForBrowser(browser())
                                 ->GetWidget()
                                 ->GetWindowBoundsInScreen();
  gfx::Rect glic_bounds = browser_bounds;

  // Shift the bounds so they overlap by exactly 46%.
  // Without the fix: 10 * 46 / 100 = 4 (integer division) -> 40%
  // With the fix: 10.0 * 46 / 100 = 4.6 -> round(4.6) = 5 -> 50%
  int overlap_height = browser_bounds.height() * 46 / 100;
  int y_offset = browser_bounds.height() - overlap_height;
  glic_bounds.Offset(0, y_offset);

  glic_service->metrics()->OnGlicWindowClose(browser(), std::nullopt,
                                             glic_bounds);

  histogram_tester.ExpectUniqueSample("Glic.PercentOverlapWithBrowser.OnClose",
                                      PercentOverlap::k50, 1);
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
IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTestWithCaptureRegion,
                       SelectionUsedFromController) {
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
  controller->Show();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return controller->state() == SelectionOverlayController::State::kOverlay;
  }));
  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->AdjustRegion(selection::SelectedRegion::New(
          base::UnguessableToken::Create(), gfx::RectF(10, 10, 10, 10)));

  auto* tab_interface = tabs::TabInterface::GetFromContents(web_contents);
  auto* instance = GetInstanceForTab(browser()->profile(), tab_interface);

  instance->host()
      .instance_metrics_backwards_compatibility()
      .OnUserInputSubmitted(mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 1, 1);

  // Submit another input, should still log 1.
  instance->host()
      .instance_metrics_backwards_compatibility()
      .OnUserInputSubmitted(mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 1, 2);

  // Close the overlay.
  SelectionOverlayController::FromTabWebContents(web_contents)->Close();

  // Submit another input, should log 0.
  instance->host()
      .instance_metrics_backwards_compatibility()
      .OnUserInputSubmitted(mojom::WebClientMode::kText);
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
