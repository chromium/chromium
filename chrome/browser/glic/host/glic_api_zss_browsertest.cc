// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/fake_contextual_cueing_service.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

namespace glic {

class GlicApiTestWithContextualCueing : public GlicApiBrowserTest {
 public:
  GlicApiTestWithContextualCueing()
      : GlicApiBrowserTest(GlicTestJsPath("./glic_api_zss_browsertest.js")) {
    contextual_cueing_features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicWebClientLoadTimes,
          {
              // Shorten load timeouts.
              {features::kGlicPreLoadingTimeMs.name, "20"},
              {features::kGlicMinLoadingTimeMs.name, "40"},
          }},
         {kGlicZeroStateSuggestions, {}},
         {kContextualCueing, {}}},
        /*disabled_features=*/
        {});
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* browser_context) override {
#if BUILDFLAG(IS_CHROMEOS)
    if (!ash::IsUserBrowserContext(browser_context)) {
      return;
    }
#endif
    fake_cueing_service_ = static_cast<FakeContextualCueingService*>(
        ContextualCueingServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeContextualCueingService>();
            })));

    GlicApiBrowserTest::SetUpBrowserContextKeyedServices(browser_context);
  }

  void SetUpOnMainThread() override {
    GlicApiBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(content::NavigateToURL(
        GetTabListInterface()->GetActiveTab()->GetContents(),
        GetTestUrl("page.html")));
  }

  void TearDownOnMainThread() override {
    fake_cueing_service_ = nullptr;
    GlicApiBrowserTest::TearDownOnMainThread();
  }

  FakeContextualCueingService* fake_cueing_service() {
    return fake_cueing_service_;
  }

 private:
  raw_ptr<FakeContextualCueingService> fake_cueing_service_;
  base::test::ScopedFeatureList contextual_cueing_features_;
};

IN_PROC_BROWSER_TEST_F(GlicApiTestWithContextualCueing,
                       testGetZeroStateSuggestionsForFocusedTabApi) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  EXPECT_GE(fake_cueing_service()->focused_tab_call_count(), 1);
}

IN_PROC_BROWSER_TEST_F(
    GlicApiTestWithContextualCueing,
    testGetZeroStateSuggestionsForFocusedTabFailsWhenHidden) {
  ASSERT_OK(OpenGlicForActiveTab());
  PreventDeletionOnClose();
  ExecuteJsTest();
}

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/533085229): Re-enable on Android once close flakiness is
// fixed.
#define MAYBE_testNoZssWarmingStateMachine DISABLED_testNoZssWarmingStateMachine
#else
#define MAYBE_testNoZssWarmingStateMachine testNoZssWarmingStateMachine
#endif
IN_PROC_BROWSER_TEST_F(GlicApiTestWithContextualCueing,
                       MAYBE_testNoZssWarmingStateMachine) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // 1. Initial Open via Blocked Source (kPromotionPage) -> disables warming.
  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kPromotionPage);
  ASSERT_OK(WaitForGlicOpen());

  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab1);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 0);

  // 2. Simulate showing Glic again after closing via an explicit source
  // (kTopChromeButton) -> resets zss_warming_enabled_ = true and runs
  // warming.
  PreventBlankDeletionOnClose(instance);
  instance->CloseAllEmbedders();
  ASSERT_OK(WaitForGlicClose());

  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kTopChromeButton);
  ASSERT_OK(WaitForGlicOpen());
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 1);

  // 3. If the web client explicitly requests ZSS, it should still get
  // results.
  ExecuteJsTest();
  // The JS request makes another call to the backend service, bringing the
  // total call count to 2.
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 2);
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithContextualCueing,
                       testNoZssWarmingStateMachineImplicitPreservesDisabled) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // 1. Initial Open via Blocked Source (kPromotionPage) -> disables warming.
  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kPromotionPage);
  ASSERT_OK(WaitForGlicOpen());

  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab1);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 0);

  // 2. Simulate showing Glic again after closing via an implicit source
  // (kTabRestore) -> preserves disabled state (zss_warming_enabled_ == false).
  PreventBlankDeletionOnClose(instance);
  instance->CloseAllEmbedders();
  ASSERT_OK(WaitForGlicClose());

  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kTabRestore);
  ASSERT_OK(WaitForGlicOpen());
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 0);

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithContextualCueing,
                       testNoZssWarmingStateMachineImplicitPreservesEnabled) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // 1. Initial Open via Explicit Source (kTopChromeButton) -> warming enabled.
  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kTopChromeButton);
  ASSERT_OK(WaitForGlicOpen());

  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab1);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 1);

  // 2. Simulate showing Glic again after closing via an implicit source
  // (kTabRestore) -> preserves enabled state (zss_warming_enabled_ == true)
  // and runs warming on open.
  PreventBlankDeletionOnClose(instance);
  instance->CloseAllEmbedders();
  ASSERT_OK(WaitForGlicClose());

  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kTabRestore);
  ASSERT_OK(WaitForGlicOpen());
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 2);

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithContextualCueing,
                       testGetZeroStateSuggestionsApi) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  EXPECT_EQ(fake_cueing_service()->pinned_tabs_call_count(), 1);
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithContextualCueing,
                       testGetZeroStateSuggestionsUnsubscribeAndResubscribe) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  EXPECT_EQ(fake_cueing_service()->pinned_tabs_call_count(), 1);
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithContextualCueing,
                       testGetZeroStateSuggestionsMultipleNavigations) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicApiTestWithContextualCueing,
                       testGetZeroStateSuggestionsFailsWhenHidden) {
  ASSERT_OK(OpenGlicForActiveTab());
  PreventDeletionOnClose(nullptr);
  ExecuteJsTest();

  int initial_calls = fake_cueing_service()->focused_tab_call_count();

  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      GetTestUrl("page.html?new")));

  ContinueJsTest();
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), initial_calls);
}

}  // namespace glic
