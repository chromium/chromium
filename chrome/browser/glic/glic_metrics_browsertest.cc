// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/command_line.h"
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
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL, "about:blank");
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

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTestWithMessageFirstFre,
                       BackgroundCreation_FreShown_MessageFirstFreEnabled) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // 1. Create a background tab.
  int initial_tab_count = browser()->tab_strip_model()->count();
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, /*foreground=*/false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), initial_tab_count + 1);

  tabs::TabInterface* background_tab =
      browser()->tab_strip_model()->GetTabAtIndex(initial_tab_count);
  ASSERT_TRUE(background_tab);
  ASSERT_FALSE(background_tab->IsActivated());

  // 2. Invoke for the background tab.
  GlicInvokeOptions options(mojom::InvocationSource::kNavigationCapture);
  options.target.conversation = NewConversation{};
  options.target.surface = background_tab;

  glic_service->Invoke(std::move(options));

  // 3. Verify no Glic.Fre.Shown logged yet.
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Shown"), 0);

  // 4. Activate the tab to reveal the side panel (and trigger FRE).
  browser()->tab_strip_model()->ActivateTabAt(initial_tab_count);
  ASSERT_TRUE(background_tab->IsActivated());

  // 5. Verify Glic.Fre.Shown IS logged then.
  auto* coordinator = GlicSidePanelCoordinator::GetForTab(background_tab);
  ASSERT_TRUE(coordinator);
  ASSERT_TRUE(base::test::RunUntil([&]() { return coordinator->IsShowing(); }));

  // V1 metrics ARE logged on reveal for background creation due to unified
  // condition.
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Shown"), 1);
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
      mojom::InvocationSource::kOsHotkey);

  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.ToggleSource",
                                      mojom::InvocationSource::kOsHotkey, 1);
  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.OpenSource",
                                      mojom::InvocationSource::kOsHotkey, 1);

  // Close the floaty panel.
  glic_service->instance_coordinator().Toggle(
      /*browser=*/nullptr, /*prevent_close=*/false,
      mojom::InvocationSource::kOsHotkey);

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

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest, BackgroundCreationThenReveal) {
  base::HistogramTester histogram_tester;
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // 1. Open Glic in active tab (Tab 1) to create an instance.
  glic_service->ToggleUI(browser(), /*prevent_close=*/false,
                         mojom::InvocationSource::kOsButton);

  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab1);

  // 2. Create a background tab (Tab 2).
  int initial_tab_count = browser()->tab_strip_model()->count();
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, /*foreground=*/false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), initial_tab_count + 1);

  tabs::TabInterface* background_tab =
      browser()->tab_strip_model()->GetTabAtIndex(initial_tab_count);
  ASSERT_TRUE(background_tab);
  ASSERT_FALSE(background_tab->IsActivated());

  // 3. Get instance for Tab 1 and call Show for Tab 2 in background.
  auto* instance =
      static_cast<GlicInstanceImpl*>(glic_service->GetInstanceForTab(tab1));
  ASSERT_TRUE(instance);

  SidePanelShowOptions side_panel_options{*background_tab};
  auto show_options = ShowOptions{side_panel_options};
  instance->Show(show_options);

  // 4. Verify no OnOpen sample is added yet for Tab 2.
  histogram_tester.ExpectTotalCount("Glic.Instance.SidePanel.OpenSource", 1);
  histogram_tester.ExpectBucketCount("Glic.Instance.SidePanel.OpenSource",
                                     mojom::InvocationSource::kOsButton, 1);

  // 5. Activate the tab to reveal the side panel.
  browser()->tab_strip_model()->ActivateTabAt(initial_tab_count);
  ASSERT_TRUE(background_tab->IsActivated());

  // 6. Verify OnOpen sample is recorded for Tab 2.
  auto* coordinator = GlicSidePanelCoordinator::GetForTab(background_tab);
  ASSERT_TRUE(coordinator);
  ASSERT_TRUE(base::test::RunUntil([&]() { return coordinator->IsShowing(); }));

  histogram_tester.ExpectTotalCount("Glic.Instance.SidePanel.OpenSource", 2);
  histogram_tester.ExpectBucketCount("Glic.Instance.SidePanel.OpenSource",
                                     mojom::InvocationSource::kTopChromeButton,
                                     1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest, TabSwitchingSuppressesOnOpen) {
  base::HistogramTester histogram_tester;
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // 1. Open Glic in active tab (Tab 1).
  glic_service->ToggleUI(browser(), /*prevent_close=*/false,
                         mojom::InvocationSource::kOsButton);

  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab1);

  auto* coordinator1 = GlicSidePanelCoordinator::GetForTab(tab1);
  ASSERT_TRUE(coordinator1);

  // Simulate user input to prevent unbinding on tab switch.
  auto* instance =
      static_cast<GlicInstanceImpl*>(glic_service->GetInstanceForTab(tab1));
  ASSERT_TRUE(instance);
  instance->OnUserInputSubmitted(mojom::WebClientMode::kText);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return coordinator1->IsShowing(); }));

  histogram_tester.ExpectUniqueSample("Glic.Instance.SidePanel.OpenSource",
                                      mojom::InvocationSource::kOsButton, 1);

  // 2. Create a background tab (Tab 2).
  int initial_tab_count = browser()->tab_strip_model()->count();
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, /*foreground=*/false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), initial_tab_count + 1);

  // 3. Switch to Tab 2. Tab 1's Glic should become inactive/hidden.
  browser()->tab_strip_model()->ActivateTabAt(initial_tab_count);

  // 4. Switch back to Tab 1.
  browser()->tab_strip_model()->ActivateTabAt(initial_tab_count - 1);

  // Wait for it to become visible again.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return coordinator1->IsShowing(); }));

  // 5. Verify that NO duplicate OnOpen sample is recorded for Tab 1.
  // We check the bucket count for kOsButton because Tab 2 might have logged
  // a sample with the default kTopChromeButton when it was activated.
  histogram_tester.ExpectBucketCount("Glic.Instance.SidePanel.OpenSource",
                                     mojom::InvocationSource::kOsButton, 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       BackgroundCreationThenReveal_InvokeVariant) {
  base::HistogramTester histogram_tester;
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // 1. Open Glic in active tab (Tab 1) via Invoke.
  GlicInvokeOptions options1(mojom::InvocationSource::kNavigationCapture);
  options1.target.conversation = NewConversation{};
  options1.target.surface = browser()->GetActiveTabInterface();
  glic_service->Invoke(std::move(options1));

  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab1);

  auto* coordinator = GlicSidePanelCoordinator::GetForTab(tab1);
  ASSERT_TRUE(coordinator);
  ASSERT_TRUE(base::test::RunUntil([&]() { return coordinator->IsShowing(); }));

  histogram_tester.ExpectUniqueSample(
      "Glic.Instance.SidePanel.OpenSource",
      mojom::InvocationSource::kNavigationCapture, 1);

  // 2. Create a background tab (Tab 2).
  int initial_tab_count = browser()->tab_strip_model()->count();
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, /*foreground=*/false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), initial_tab_count + 1);

  tabs::TabInterface* background_tab =
      browser()->tab_strip_model()->GetTabAtIndex(initial_tab_count);
  ASSERT_TRUE(background_tab);
  ASSERT_FALSE(background_tab->IsActivated());

  // 3. Get instance for Tab 1 and call Show for Tab 2 in background.
  auto* instance =
      static_cast<GlicInstanceImpl*>(glic_service->GetInstanceForTab(tab1));
  ASSERT_TRUE(instance);

  SidePanelShowOptions side_panel_options{*background_tab};
  auto show_options = ShowOptions{side_panel_options};
  instance->Show(show_options);

  // 4. Verify no OnOpen sample is added yet for Tab 2.
  histogram_tester.ExpectTotalCount("Glic.Instance.SidePanel.OpenSource", 1);

  // 5. Activate the tab to reveal the side panel.
  browser()->tab_strip_model()->ActivateTabAt(initial_tab_count);
  ASSERT_TRUE(background_tab->IsActivated());

  // 6. Verify OnOpen sample is recorded for Tab 2.
  auto* coordinator2 = GlicSidePanelCoordinator::GetForTab(background_tab);
  ASSERT_TRUE(coordinator2);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return coordinator2->IsShowing(); }));

  histogram_tester.ExpectTotalCount("Glic.Instance.SidePanel.OpenSource", 2);
  histogram_tester.ExpectBucketCount("Glic.Instance.SidePanel.OpenSource",
                                     mojom::InvocationSource::kTopChromeButton,
                                     1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       TabSwitchingSuppressesOnOpen_InvokeVariant) {
  base::HistogramTester histogram_tester;
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // 1. Open Glic in active tab (Tab 1) via Invoke.
  GlicInvokeOptions options1(mojom::InvocationSource::kNavigationCapture);
  options1.target.conversation = NewConversation{};
  options1.target.surface = browser()->GetActiveTabInterface();
  glic_service->Invoke(std::move(options1));

  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab1);

  auto* coordinator1 = GlicSidePanelCoordinator::GetForTab(tab1);
  ASSERT_TRUE(coordinator1);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return coordinator1->IsShowing(); }));

  histogram_tester.ExpectUniqueSample(
      "Glic.Instance.SidePanel.OpenSource",
      mojom::InvocationSource::kNavigationCapture, 1);

  // Simulate user input to prevent unbinding on tab switch.
  auto* instance =
      static_cast<GlicInstanceImpl*>(glic_service->GetInstanceForTab(tab1));
  ASSERT_TRUE(instance);
  instance->OnUserInputSubmitted(mojom::WebClientMode::kText);

  // 2. Create a background tab (Tab 2).
  int initial_tab_count = browser()->tab_strip_model()->count();
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, /*foreground=*/false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), initial_tab_count + 1);

  // 3. Switch to Tab 2. Tab 1's Glic should become inactive/hidden.
  browser()->tab_strip_model()->ActivateTabAt(initial_tab_count);

  // 4. Switch back to Tab 1.
  browser()->tab_strip_model()->ActivateTabAt(initial_tab_count - 1);

  // Wait for it to become visible again.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return coordinator1->IsShowing(); }));

  // 5. Verify that NO duplicate OnOpen sample is recorded for Tab 1.
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.SidePanel.OpenSource",
      mojom::InvocationSource::kNavigationCapture, 1);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest, FloatyDetachAttachDetach) {
  base::HistogramTester histogram_tester;
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // 1. Open side panel in active tab (Tab 1).
  glic_service->ToggleUI(browser(), /*prevent_close=*/false,
                         mojom::InvocationSource::kOsButton);

  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab1);

  auto* instance =
      static_cast<GlicInstanceImpl*>(glic_service->GetInstanceForTab(tab1));
  ASSERT_TRUE(instance);

  // 2. Detach to Floaty.
  instance->Detach(*tab1);

  // Floaty logs OnOpen. Detach uses kTopChromeButton as default.
  histogram_tester.ExpectBucketCount("Glic.Instance.Floaty.OpenSource",
                                     mojom::InvocationSource::kTopChromeButton,
                                     1);

  // 3. Attach floaty back to side panel. This deactivates Floaty and should
  // reset its flag.
  instance->Attach(tab1->GetHandle());

  // 4. Detach to Floaty AGAIN.
  instance->Detach(*tab1);

  // 5. Verify Floaty logged OnOpen again.
  histogram_tester.ExpectBucketCount("Glic.Instance.Floaty.OpenSource",
                                     mojom::InvocationSource::kTopChromeButton,
                                     2);
}

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest,
                       FloatySwitchConversationLogsOnOpen) {
  base::HistogramTester histogram_tester;
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  // 1. Open side panel.
  glic_service->ToggleUI(browser(), /*prevent_close=*/false,
                         mojom::InvocationSource::kOsButton);

  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab1);

  auto* instance =
      static_cast<GlicInstanceImpl*>(glic_service->GetInstanceForTab(tab1));
  ASSERT_TRUE(instance);

  // 2. Detach to Floaty.
  instance->Detach(*tab1);
  histogram_tester.ExpectBucketCount("Glic.Instance.Floaty.OpenSource",
                                     mojom::InvocationSource::kTopChromeButton,
                                     1);

  // 3. Switch conversation on the instance.
  FloatingShowOptions floating_options{gfx::Rect(), tab1->GetHandle()};
  ShowOptions switch_options(floating_options);
  switch_options.invocation_source = mojom::InvocationSource::kOsHotkey;

  // Call SwitchConversation to test the specific fix.
  instance->SwitchConversation(switch_options, mojom::ConversationInfo::New(),
                               base::DoNothing());

  // 4. Verify Floaty logged OnOpen again for the new conversation.
  histogram_tester.ExpectTotalCount("Glic.Instance.Floaty.OpenSource", 2);
  histogram_tester.ExpectBucketCount("Glic.Instance.Floaty.OpenSource",
                                     mojom::InvocationSource::kOsHotkey, 1);
}

}  // namespace
}  // namespace glic
