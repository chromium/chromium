// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This is a browser test for Omnibox triggered prerendering. This is
// implemented as an interactive UI test so that it can emulate navigation
// initiated by URL typed on the Omnibox.
class PrerenderOmniboxUIBrowserTest : public InProcessBrowserTest,
                                      public content::WebContentsObserver {
 public:
  PrerenderOmniboxUIBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PrerenderOmniboxUIBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kOmniboxTriggerForPrerender2);
  }

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    InProcessBrowserTest::SetUp();
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
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  // Returns last committed page transition type. This value is only
  // meaningful after calling Observe(GetActiveWebContents()) in the test case
  // and after DidFinishNavigation.
  ui::PageTransition GetLastPageTransitionType() {
    return last_finished_page_transition_type_;
  }

  // Returns last committed page is prerendered or not. This value is only
  // meaningful after calling Observe(GetActiveWebContents()) in the test case
  // and after DidFinishNavigation.
  bool IsPrerenderingNavigation() { return is_prerendering_page_; }

 protected:
  void StartOmniboxNavigationAndWaitForActivation(const GURL& url) {
    SetOmniboxText(url.spec());
    PressEnterAndWaitForActivation(url);
  }

  predictors::AutocompleteActionPredictor* GetAutocompleteActionPredictor() {
    return predictors::AutocompleteActionPredictorFactory::GetForProfile(
        browser()->profile());
  }

 private:
  OmniboxView* omnibox() {
    return browser()->window()->GetLocationBar()->GetOmniboxView();
  }

  void FocusOmnibox() {
    // If the omnibox already has focus, just notify OmniboxTabHelper.
    if (omnibox()->model()->has_focus()) {
      OmniboxTabHelper::FromWebContents(GetActiveWebContents())
          ->OnFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                           OMNIBOX_FOCUS_CHANGE_EXPLICIT);
    } else {
      browser()->window()->GetLocationBar()->FocusLocation(false);
    }
  }

  void SetOmniboxText(const std::string& text) {
    FocusOmnibox();
    // Enter user input mode to prevent spurious unelision.
    omnibox()->model()->SetInputInProgress(true);
    omnibox()->OnBeforePossibleChange();
    omnibox()->SetUserText(base::UTF8ToUTF16(text), true);
    omnibox()->OnAfterPossibleChange(true);
  }

  void PressEnter() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const Browser* browser, bool ctrl_key) {
              EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
                  browser, ui::VKEY_RETURN, ctrl_key, false, false, false));
            },
            browser(), false));
  }

  // Presses enter and waits for Activation
  void PressEnterAndWaitForActivation(const GURL& url) {
    content::test::PrerenderHostObserver prerender_observer(
        *GetActiveWebContents(), url);
    PressEnter();
    prerender_observer.WaitForActivation();
  }

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    last_finished_page_transition_type_ =
        navigation_handle->GetPageTransition();
    is_prerendering_page_ = navigation_handle->IsPrerenderedPageActivation();
  }

  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::PageTransition last_finished_page_transition_type_;
  bool is_prerendering_page_;
};

// This test covers the path from starting a omnibox triggered prerendering
// by AutocompleteActionPredictor, and simulate the omnibox input to check
// that prerendering can be activated successfully and the page transition type
// is correctly set as (ui::PAGE_TRANSITION_TYPED |
// ui::PAGE_TRANSITION_FROM_ADDRESS_BAR).
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxUIBrowserTest,
                       PrerenderingByAutocompleteActionPredictorCanActivate) {
  Observe(GetActiveWebContents());
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  // Attempt to prerender a direct URL input.
  ASSERT_TRUE(GetAutocompleteActionPredictor());
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?prerender");
  GetAutocompleteActionPredictor()->StartPrerendering(
      kPrerenderingUrl, *GetActiveWebContents(), gfx::Size(50, 50));
  StartOmniboxNavigationAndWaitForActivation(kPrerenderingUrl);
  EXPECT_EQ(static_cast<int>(GetLastPageTransitionType()),
            static_cast<int>(ui::PAGE_TRANSITION_TYPED |
                             ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(IsPrerenderingNavigation());
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(), kPrerenderingUrl);
}

// This test starts two different url prerendering by
// AutocompleteActionPredictor, and checks that the second one is going to
// cancel the first one.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxUIBrowserTest,
                       CancelAutocompleteActionPredictorOldPrerendering) {
  Observe(GetActiveWebContents());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  // Attempt to prerender a direct URL input.
  ASSERT_TRUE(GetAutocompleteActionPredictor());
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?prerender");
  content::test::PrerenderHostObserver old_prerender_observer(
      *GetActiveWebContents(), kPrerenderingUrl);
  const GURL kNewUrl = embedded_test_server()->GetURL("/empty.html?newUrl");
  GetAutocompleteActionPredictor()->StartPrerendering(
      kPrerenderingUrl, *GetActiveWebContents(), gfx::Size(50, 50));
  EXPECT_NE(prerender_helper().GetHostForUrl(kPrerenderingUrl),
            content::RenderFrameHost::kNoFrameTreeNodeId);
  GetAutocompleteActionPredictor()->StartPrerendering(
      kNewUrl, *GetActiveWebContents(), gfx::Size(50, 50));

  old_prerender_observer.WaitForDestroyed();
  StartOmniboxNavigationAndWaitForActivation(kNewUrl);

  EXPECT_TRUE(IsPrerenderingNavigation());
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(), kNewUrl);
}

// This test starts same url prerendering twice by AutocompleteActionPredictor,
// and checks that the second one will not trigger cancellation mechanism.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxUIBrowserTest,
                       AutocompleteActionPredictorSameURL) {
  base::HistogramTester histogram_tester;
  Observe(GetActiveWebContents());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  // Attempt to prerender a direct URL input.
  ASSERT_TRUE(GetAutocompleteActionPredictor());
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?prerender");
  GetAutocompleteActionPredictor()->StartPrerendering(
      kPrerenderingUrl, *GetActiveWebContents(), gfx::Size(50, 50));
  GetAutocompleteActionPredictor()->StartPrerendering(
      kPrerenderingUrl, *GetActiveWebContents(), gfx::Size(50, 50));

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      /*PrerenderHost::FinalStatus::kTriggerDestroyed*/ 1, 0);

  StartOmniboxNavigationAndWaitForActivation(kPrerenderingUrl);

  EXPECT_TRUE(IsPrerenderingNavigation());
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(), kPrerenderingUrl);
}

// Tests that NavigationHandle::IsRendererInitiated() returns RendererInitiated
// = false correctly.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxUIBrowserTest,
                       NavigationHandleIsRendererInitiatedFalse) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?prerender");
  {
    base::RunLoop run_loop;
    content::DidFinishNavigationObserver observer(
        GetActiveWebContents(),
        base::BindLambdaForTesting(
            [&run_loop](content::NavigationHandle* navigation_handle) {
              EXPECT_TRUE(navigation_handle->IsInPrerenderedMainFrame());
              EXPECT_FALSE(navigation_handle->IsRendererInitiated());
              run_loop.Quit();
            }));
    GetAutocompleteActionPredictor()->StartPrerendering(
        kPrerenderingUrl, *GetActiveWebContents(), gfx::Size(50, 50));
    run_loop.Run();
  }
  StartOmniboxNavigationAndWaitForActivation(kPrerenderingUrl);
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(), kPrerenderingUrl);
}

}  // namespace
