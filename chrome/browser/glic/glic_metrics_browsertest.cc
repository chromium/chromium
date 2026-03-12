// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/ozone/public/ozone_platform.h"
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

  // Open the side panel
  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.ToggleSource",
                                      mojom::InvocationSource::kOsButton, 1);
  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);

  // Close the side panel
  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.ToggleSource",
                                      mojom::InvocationSource::kOsButton, 2);
  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       ToggleAndOpenSourceMetrics_Floaty) {
  ASSERT_TRUE(GlicEnabling::IsMultiInstanceEnabled());

  base::HistogramTester histogram_tester;

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // First toggle the UI to create the floaty instance.
  glic_service->window_controller().Toggle(
      /*browser=*/nullptr, /*prevent_close=*/false,
      mojom::InvocationSource::kOsHotkey, std::nullopt, false, std::nullopt);

  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.ToggleSource",
                                      mojom::InvocationSource::kOsHotkey, 1);
  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.OpenSource",
                                      mojom::InvocationSource::kOsHotkey, 1);

  // Close the floaty panel.
  glic_service->window_controller().Toggle(
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

}  // namespace
}  // namespace glic
