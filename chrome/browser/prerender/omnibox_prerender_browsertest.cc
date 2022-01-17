// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace {

class OmniboxPrerenderBrowserTest : public PlatformBrowserTest {
 public:
  OmniboxPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &OmniboxPrerenderBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kOmniboxTriggerForPrerender2);
  }

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  Profile* GetProfile() {
#if BUILDFLAG(IS_ANDROID)
    return chrome_test_utils::GetProfile(this);
#else
    return browser()->profile();
#endif
  }

  predictors::AutocompleteActionPredictor* GetAutocompleteActionPredictor() {
    Profile* profile = GetProfile();
    return predictors::AutocompleteActionPredictorFactory::GetForProfile(
        profile);
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test class uses the kPrerender2 default value, which is true for Android
// and false for others. In contrast, OmniboxPrerenderBrowserTest enables
// kPrerender2 by PrerenderTestHelper.
class OmniboxPrerenderDefaultPrerender2BrowserTest
    : public PlatformBrowserTest {
 public:
  OmniboxPrerenderDefaultPrerender2BrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kOmniboxTriggerForPrerender2);
  }

  void SetUp() override { PlatformBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that Prerender2 cannot be triggered when preload setting is disabled.
IN_PROC_BROWSER_TEST_F(OmniboxPrerenderBrowserTest, DisableNetworkPrediction) {
  // Disable network prediction.
  PrefService* prefs = GetProfile()->GetPrefs();
  prefetch::SetPreloadPagesState(prefs,
                                 prefetch::PreloadPagesState::kNoPreloading);
  ASSERT_FALSE(prefetch::IsSomePreloadingEnabled(*prefs));

  // Attempt to prerender a direct URL input.
  auto* predictor = GetAutocompleteActionPredictor();
  ASSERT_TRUE(predictor);
  content::WebContents* web_contents = GetActiveWebContents();
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");
  predictor->StartPrerendering(prerender_url, *web_contents, gfx::Size(50, 50));

  // Since preload setting is disabled, prerender shouldn't be triggered.
  base::RunLoop().RunUntilIdle();
  int host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_EQ(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  // Re-enable the setting.
  prefetch::SetPreloadPagesState(
      prefs, prefetch::PreloadPagesState::kStandardPreloading);
  ASSERT_TRUE(prefetch::IsSomePreloadingEnabled(*prefs));

  content::test::PrerenderHostRegistryObserver registry_observer(*web_contents);
  // Attempt to trigger prerendering again.
  predictor->StartPrerendering(prerender_url, *web_contents, gfx::Size(50, 50));

  // Since preload setting is enabled, prerender should be triggered
  // successfully.
  registry_observer.WaitForTrigger(prerender_url);
  host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
}

// Verifies that prerendering functions in document are properly exposed.
// TODO(https://crbug.com/1286374): test is flaky.
IN_PROC_BROWSER_TEST_F(
    OmniboxPrerenderBrowserTest,
    DISABLED_PrerenderFunctionsProperlyExportedWhenInitiatedByOmnibox) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  EXPECT_EQ(true,
            EvalJs(GetActiveWebContents(), "document.prerendering === false"));
  EXPECT_EQ(
      0,
      EvalJs(GetActiveWebContents(),
             "performance.getEntriesByType('navigation')[0].activationStart"));

  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/prerender/onprerendering_check.html");

  GetAutocompleteActionPredictor()->StartPrerendering(
      kPrerenderingUrl, *GetActiveWebContents(), gfx::Size(50, 50));

  int host_id = prerender_helper().GetHostForUrl(kPrerenderingUrl);
  content::RenderFrameHost* prerender_frame_host =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);
  EXPECT_EQ(true,
            EvalJs(prerender_frame_host, "document.prerendering === true"));

  // Simulate a browser-initiated navigation.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), kPrerenderingUrl);

  GetActiveWebContents()->OpenURL(content::OpenURLParams(
      kPrerenderingUrl, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
  prerender_observer.WaitForActivation();

  EXPECT_EQ(true, EvalJs(prerender_frame_host,
                         "onprerenderingchange_observed_promise"));
  EXPECT_LT(
      0.0,
      EvalJs(prerender_frame_host,
             "performance.getEntriesByType('navigation')[0].activationStart")
          .ExtractDouble());
}

// Verifies that the exportation of prerendering functions in the document is
// handled properly when Prerender2 is set to be the default value. For android,
// on which Prerender2 is enabled,  those functions are expected to be exported,
// while the functions are not supposed to be exported on other platforms.
IN_PROC_BROWSER_TEST_F(OmniboxPrerenderDefaultPrerender2BrowserTest,
                       PrerenderFunctionsCheckWithDefaultFlag) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(true,
            EvalJs(GetActiveWebContents(), "document.prerendering === false"));
  EXPECT_EQ(0, EvalJs(GetActiveWebContents(),
                      "performance.getEntriesByType('navigation')[0]."
                      "activationStart"));
  EXPECT_EQ(true, EvalJs(GetActiveWebContents(),
                         "'onprerenderingchange' in document"));
#else
  EXPECT_EQ(true, EvalJs(GetActiveWebContents(),
                         "document.prerendering === undefined"));
  EXPECT_EQ(true, EvalJs(GetActiveWebContents(),
                         "performance.getEntriesByType('navigation')[0]."
                         "activationStart === undefined"));
  EXPECT_EQ(true, EvalJs(GetActiveWebContents(),
                         "document.onprerenderingchange === undefined"));
#endif
}

}  // namespace
