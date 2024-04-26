// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
#include "chrome/browser/android/customtabs/tab_interaction_recorder_android.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

const char kHistogramBackForwardCachePageWithFormStorable[] =
    "BackForwardCache.PageWithForm.Storable";
const char kHistogramBackForwardCachePageWithFormRestoreResult[] =
    "BackForwardCache.PageWithForm.RestoreResult";

// Browser test that aim to test whether bits regarding tab interaction and form
// seen is correctly recorded during navigation. This test is largely
// referencing the ChromeBackForwardCacheBrowserTest.
class TabInteractionRecorderAndroidBrowserTest : public AndroidBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    customtabs::TabInteractionRecorderAndroid::CreateForWebContents(
        web_contents());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // For using an HTTPS server.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    // TODO(crbug.com/40285326): This fails with the field trial testing config.
    command_line->AppendSwitch("disable-field-trial-config");
  }

  // Helper functions to verify Histogram
  // BackForwardCachePageWithFormEventCounts recorded with the right samples.
  // These helper are used as enum BackForwardCachePageWithFormsEvent lives in
  // //content/browser and is not visible to this test.
  void ExpectPageWithFormSeenRecorded(int count) {
    histogram_tester_.ExpectBucketCount(
        kHistogramBackForwardCachePageWithFormStorable, /*kPageSeen*/ 0, count);
  }
  void ExpectPageWithFormStoredRecorded(int count) {
    histogram_tester_.ExpectBucketCount(
        kHistogramBackForwardCachePageWithFormStorable, /*kPageStored*/ 1,
        count);
  }
  void ExpectPageWithFormRestoredRecorded(int count) {
    histogram_tester_.ExpectBucketCount(
        kHistogramBackForwardCachePageWithFormRestoreResult, /*kRestored*/ 0,
        count);
  }
  void ExpectPageWithFormNotRestoredRecorded(int count) {
    histogram_tester_.ExpectBucketCount(
        kHistogramBackForwardCachePageWithFormRestoreResult,
        /*kNotRestored*/ 1, count);
  }

 protected:
  class TestAutofillManager : public autofill::BrowserAutofillManager {
   public:
    explicit TestAutofillManager(autofill::ContentAutofillDriver* driver)
        : autofill::BrowserAutofillManager(driver, "en-US") {}

    [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
        int min_num_awaited_calls) {
      return forms_seen_waiter_.Wait(min_num_awaited_calls);
    }

   private:
    autofill::TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {autofill::AutofillManagerEvent::kFormsSeen}};
  };

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TestAutofillManager& GetAutofillManagerInMainFrame() {
    autofill::ContentAutofillDriver* driver =
        autofill::ContentAutofillDriver::GetForRenderFrameHost(
            web_contents()->GetPrimaryMainFrame());
    return static_cast<TestAutofillManager&>(driver->GetAutofillManager());
  }
  // Returns a URL with host `host` and path "/title1.html".
  GURL GetURLWithHost(const std::string& host) {
    return embedded_test_server()->GetURL(host, "/title1.html");
  }

  GURL GetTestFormUrl() {
    return embedded_test_server()->GetURL("/autofill/autofill_test_form.html");
  }

  GURL GetTestFormUrlWithHandle(const std::string& handle) {
    return embedded_test_server()->GetURL("/autofill/autofill_test_form.html#" +
                                          handle);
  }

  GURL GetTestFormInSubFrameUrl() {
    return embedded_test_server()->GetURL(
        "/autofill/iframe_autocomplete_simple_form.html");
  }

  base::FilePath GetChromeTestDataDir() {
    return base::FilePath(FILE_PATH_LITERAL("chrome/test/data"));
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::map<base::test::FeatureRef, std::map<std::string, std::string>>
      features_with_params_;
  logging::ScopedVmoduleSwitches vmodule_switches_;
  autofill::TestAutofillManagerInjector<TestAutofillManager>
      autofill_manager_injector_;
};

IN_PROC_BROWSER_TEST_F(TabInteractionRecorderAndroidBrowserTest,
                       PageSeenAndStore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to page with test form.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetTestFormUrl()));
  EXPECT_TRUE(GetAutofillManagerInMainFrame().WaitForFormsSeen(1));

  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 2) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURLWithHost("b.com")));
  content::RenderFrameHostWrapper rfh_b(current_frame_host());

  // A is frozen in back/forward cache, page seen and store is recorded.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  content::FetchHistogramsFromChildProcesses();
  ExpectPageWithFormSeenRecorded(1);
  ExpectPageWithFormStoredRecorded(1);
  ExpectPageWithFormRestoredRecorded(0);
  ExpectPageWithFormNotRestoredRecorded(0);

  // 3) Go back
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Page A should be restored and the histogram value is recorded.
  content::FetchHistogramsFromChildProcesses();
  ExpectPageWithFormRestoredRecorded(1);
  ExpectPageWithFormNotRestoredRecorded(0);
}

IN_PROC_BROWSER_TEST_F(TabInteractionRecorderAndroidBrowserTest,
                       StoreAterSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to page with test form.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetTestFormUrl()));
  EXPECT_TRUE(GetAutofillManagerInMainFrame().WaitForFormsSeen(1));

  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 2) Perform a JS that does a same document navigation.
  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), GetTestFormUrlWithHandle("foo")));
  EXPECT_EQ(rfh_a.get(), current_frame_host());

  // Page seen is not recorded for same-document navigation.
  ExpectPageWithFormSeenRecorded(0);

  // 3) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURLWithHost("b.com")));
  content::RenderFrameHostWrapper rfh_b(current_frame_host());

  // A is frozen in back/forward cache. Page seen and page stored is recorded.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  content::FetchHistogramsFromChildProcesses();
  ExpectPageWithFormSeenRecorded(1);
  ExpectPageWithFormStoredRecorded(1);
  ExpectPageWithFormRestoredRecorded(0);
  ExpectPageWithFormNotRestoredRecorded(0);
}

IN_PROC_BROWSER_TEST_F(TabInteractionRecorderAndroidBrowserTest,
                       DoNotRecordForPageWithoutForm) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to A (no form attached).
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURLWithHost("a.com")));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 3) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURLWithHost("b.com")));
  content::RenderFrameHostWrapper rfh_b(current_frame_host());

  // A is frozen in BackforwardCache, but nothing should be recorded since
  // no page with forms attached is involved.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  content::FetchHistogramsFromChildProcesses();
  ExpectPageWithFormSeenRecorded(0);
  ExpectPageWithFormStoredRecorded(0);
  ExpectPageWithFormRestoredRecorded(0);
  ExpectPageWithFormNotRestoredRecorded(0);
}

IN_PROC_BROWSER_TEST_F(TabInteractionRecorderAndroidBrowserTest,
                       FormInSubFrameStoresInMainFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to page with form in sub frame.
  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), GetTestFormInSubFrameUrl()));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 3) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURLWithHost("b.com")));
  content::RenderFrameHostWrapper rfh_b(current_frame_host());

  // A is frozen in back/forward cache. Page seen and page stored is recorded.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  content::FetchHistogramsFromChildProcesses();
  ExpectPageWithFormSeenRecorded(1);
  ExpectPageWithFormStoredRecorded(1);
  ExpectPageWithFormRestoredRecorded(0);
  ExpectPageWithFormNotRestoredRecorded(0);
}

IN_PROC_BROWSER_TEST_F(TabInteractionRecorderAndroidBrowserTest,
                       PageDoNotStoreWhenCacheDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to page with test form.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetTestFormUrl()));
  EXPECT_TRUE(GetAutofillManagerInMainFrame().WaitForFormsSeen(1));

  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // Disable the backforward cache for rfh_a will result in failed to store.
  content::BackForwardCache::DisableForRenderFrameHost(
      rfh_a.get(), back_forward_cache::DisabledReason(
                       back_forward_cache::DisabledReasonId::kUnknown));

  // 2) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURLWithHost("b.com")));
  content::RenderFrameHostWrapper rfh_b(current_frame_host());
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Page A should be seen as forms attached, not be stored and the histogram
  // value is recorded.
  content::FetchHistogramsFromChildProcesses();
  ExpectPageWithFormSeenRecorded(1);
  ExpectPageWithFormStoredRecorded(0);
  ExpectPageWithFormRestoredRecorded(0);
  ExpectPageWithFormNotRestoredRecorded(0);
}

IN_PROC_BROWSER_TEST_F(TabInteractionRecorderAndroidBrowserTest,
                       PageDoNotRestoreWhenCacheFlushed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to page with test form.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetTestFormUrl()));
  EXPECT_TRUE(GetAutofillManagerInMainFrame().WaitForFormsSeen(1));

  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 2) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURLWithHost("b.com")));
  content::RenderFrameHostWrapper rfh_b(current_frame_host());

  // Page A should be seen as forms attached, stored in cache and the histogram
  // value is recorded.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  content::FetchHistogramsFromChildProcesses();
  ExpectPageWithFormSeenRecorded(1);
  ExpectPageWithFormStoredRecorded(1);
  ExpectPageWithFormRestoredRecorded(0);
  ExpectPageWithFormNotRestoredRecorded(0);

  web_contents()->GetController().GetBackForwardCache().Flush();
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back. Page is not restored and histogram is recorded.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  content::FetchHistogramsFromChildProcesses();
  ExpectPageWithFormSeenRecorded(1);
  ExpectPageWithFormStoredRecorded(1);
  ExpectPageWithFormRestoredRecorded(0);
  ExpectPageWithFormNotRestoredRecorded(1);
}
