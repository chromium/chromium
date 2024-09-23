// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/platform_thread.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/switches.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/public/test/url_loader_monitor.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/scoped_mutually_exclusive_feature_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"

namespace {

const char kExpectedPurposeHeaderOnPrefetch[] = "Purpose";
using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using prerender::test_utils::DestructionWaiter;
using prerender::test_utils::TestPrerender;
using task_manager::browsertest_util::WaitForTaskManagerRows;
using ukm::builders::Preloading_Attempt;

std::string CreateServerRedirect(const std::string& dest_url) {
  const char* const kServerRedirectBase = "/server-redirect?";
  return kServerRedirectBase + base::EscapeQueryParamValue(dest_url, false);
}

// This is the public key of tools/origin_trials/eftest.key, used to validate
// origin trial tokens generated by tools/origin_trials/generate_token.py.
static constexpr char kOriginTrialPublicKeyForTesting[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

enum class SplitCacheTestCase {
  kDisabled,
  kEnabledTripleKeyed,
  kEnabledTriplePlusCrossSiteMainFrameNavBool,
  kEnabledTriplePlusMainFrameNavInitiator,
  kEnabledTriplePlusNavInitiator
};

const struct {
  const SplitCacheTestCase test_case;
  base::test::FeatureRef feature;
} kTestCaseToFeatureMapping[] = {
    {SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
     net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean},
    {SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
     net::features::kSplitCacheByMainFrameNavigationInitiator},
    {SplitCacheTestCase::kEnabledTriplePlusNavInitiator,
     net::features::kSplitCacheByNavigationInitiator}};

}  // namespace

namespace prerender {

const char k302RedirectPage[] = "/prerender/302_redirect.html";
const char kPrefetchCookiePage[] = "/prerender/cookie.html";
const char kPrefetchFromSubframe[] = "/prerender/prefetch_from_subframe.html";
const char kPrefetchImagePage[] = "/prerender/prefetch_image.html";
const char kPrefetchJpeg[] = "/prerender/image.jpeg";
const char kPrefetchLoaderPath[] = "/prerender/prefetch_loader.html";
const char kPrefetchLoopPage[] = "/prerender/prefetch_loop.html";
const char kPrefetchMetaCSP[] = "/prerender/prefetch_meta_csp.html";
const char kPrefetchNostorePage[] = "/prerender/prefetch_nostore_page.html";
const char kPrefetchPage[] = "/prerender/prefetch_page.html";
const char kPrefetchPageWithFragment[] =
    "/prerender/prefetch_page.html#fragment";
const char kPrefetchPage2[] = "/prerender/prefetch_page2.html";
const char kPrefetchPageBigger[] = "/prerender/prefetch_page_bigger.html";
const char kPrefetchPageMultipleResourceTypes[] =
    "/prerender/prefetch_page_multiple_resource_types.html";
const char kPrefetchPng[] = "/prerender/image.png";
const char kPrefetchPng2[] = "/prerender/image2.png";
const char kPrefetchPngRedirect[] = "/prerender/image-redirect.png";
const char kPrefetchRecursePage[] = "/prerender/prefetch_recurse.html";
const char kPrefetchResponseHeaderCSP[] =
    "/prerender/prefetch_response_csp.html";
const char kPrefetchScript[] = "/prerender/prefetch.js";
const char kPrefetchScript2[] = "/prerender/prefetch2.js";
const char kPrefetchCss[] = "/prerender/style.css";
const char kPrefetchFont[] = "/prerender/font.woff";
const char kPrefetchDownloadFile[] = "/download-test1.lib";
const char kPrefetchSubresourceRedirectPage[] =
    "/prerender/prefetch_subresource_redirect.html";
const char kServiceWorkerLoader[] = "/prerender/service_worker.html";
const char kHungPrerenderPage[] = "/prerender/hung_prerender_page.html";

// A navigation observer to wait on either a new load or a swap of a
// WebContents. On swap, if the new WebContents is still loading, wait for that
// load to complete as well. Note that the load must begin after the observer is
// attached.
class NavigationOrSwapObserver : public content::WebContentsObserver,
                                 public TabStripModelObserver {
 public:
  // Waits for either a new load or a swap of |tab_strip_model|'s active
  // WebContents.
  NavigationOrSwapObserver(TabStripModel* tab_strip_model,
                           content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        tab_strip_model_(tab_strip_model),
        did_start_loading_(false),
        number_of_loads_(1) {
    EXPECT_NE(TabStripModel::kNoTab,
              tab_strip_model->GetIndexOfWebContents(web_contents));
    tab_strip_model_->AddObserver(this);
  }

  // Waits for either |number_of_loads| loads or a swap of |tab_strip_model|'s
  // active WebContents.
  NavigationOrSwapObserver(TabStripModel* tab_strip_model,
                           content::WebContents* web_contents,
                           int number_of_loads)
      : content::WebContentsObserver(web_contents),
        tab_strip_model_(tab_strip_model),
        did_start_loading_(false),
        number_of_loads_(number_of_loads) {
    EXPECT_NE(TabStripModel::kNoTab,
              tab_strip_model->GetIndexOfWebContents(web_contents));
    tab_strip_model_->AddObserver(this);
  }

  ~NavigationOrSwapObserver() override {
    tab_strip_model_->RemoveObserver(this);
  }

  void set_did_start_loading() { did_start_loading_ = true; }

  void Wait() { loop_.Run(); }

  // content::WebContentsObserver implementation:
  void DidStartLoading() override { did_start_loading_ = true; }
  void DidStopLoading() override {
    if (!did_start_loading_)
      return;
    number_of_loads_--;
    if (number_of_loads_ == 0)
      loop_.Quit();
  }

  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kReplaced)
      return;

    auto* replace = change.GetReplace();
    if (replace->old_contents != web_contents())
      return;

    // Switch to observing the new WebContents.
    Observe(replace->new_contents);
    if (replace->new_contents->IsLoading()) {
      // If the new WebContents is still loading, wait for it to complete.
      // Only one load post-swap is supported.
      did_start_loading_ = true;
      number_of_loads_ = 1;
    } else {
      loop_.Quit();
    }
  }

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
  bool did_start_loading_;
  int number_of_loads_;
  base::RunLoop loop_;
};

content::PreloadingFailureReason ToPreloadingFailureReasonFromFinalStatus(
    FinalStatus status) {
  return static_cast<content::PreloadingFailureReason>(
      static_cast<int>(status) +
      static_cast<int>(content::PreloadingFailureReason::
                           kPreloadingFailureReasonContentEnd));
}

// Waits for a new tab to open and a navigation or swap in it.
class NewTabNavigationOrSwapObserver : public TabStripModelObserver,
                                       public BrowserListObserver {
 public:
  NewTabNavigationOrSwapObserver() {
    BrowserList::AddObserver(this);
    for (const Browser* browser : *BrowserList::GetInstance())
      browser->tab_strip_model()->AddObserver(this);
  }

  NewTabNavigationOrSwapObserver(const NewTabNavigationOrSwapObserver&) =
      delete;
  NewTabNavigationOrSwapObserver& operator=(
      const NewTabNavigationOrSwapObserver&) = delete;

  ~NewTabNavigationOrSwapObserver() override {
    BrowserList::RemoveObserver(this);
  }

  void Wait() {
    new_tab_run_loop_.Run();
    swap_observer_->Wait();
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kInserted || swap_observer_)
      return;

    content::WebContents* new_tab = change.GetInsert()->contents[0].contents;
    swap_observer_ =
        std::make_unique<NavigationOrSwapObserver>(tab_strip_model, new_tab);
    swap_observer_->set_did_start_loading();

    new_tab_run_loop_.Quit();
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    browser->tab_strip_model()->AddObserver(this);
  }

 private:
  base::RunLoop new_tab_run_loop_;
  std::unique_ptr<NavigationOrSwapObserver> swap_observer_;
};

class NoStatePrefetchBrowserTest
    : public test_utils::PrerenderInProcessBrowserTest {
 public:
  NoStatePrefetchBrowserTest() {
    feature_list_.InitAndDisableFeature(
        content_settings::features::kTrackingProtection3pcd);
  }
  NoStatePrefetchBrowserTest(const NoStatePrefetchBrowserTest&) = delete;
  NoStatePrefetchBrowserTest& operator=(const NoStatePrefetchBrowserTest&) =
      delete;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    test_utils::PrerenderInProcessBrowserTest::SetUpDefaultCommandLine(
        command_line);
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialPublicKeyForTesting);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "SpeculationRulesPrefetchWithSubresources");
  }

  void SetUpOnMainThread() override {
    test_utils::PrerenderInProcessBrowserTest::SetUpOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    link_rel_attempt_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            content::preloading_predictor::kLinkRel);
    test_timer_ = std::make_unique<base::ScopedMockElapsedTimersForTest>();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void OverrideNoStatePrefetchManagerTimeTicks() {
    // The default zero time causes the prerender manager to do strange things.
    clock_.Advance(base::Seconds(1));
    GetNoStatePrefetchManager()->SetTickClockForTesting(&clock_);
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const content::test::PreloadingAttemptUkmEntryBuilder&
  link_rel_attempt_entry_builder() {
    return *link_rel_attempt_entry_builder_;
  }

 protected:
  // Loads kPrefetchLoaderPath and specifies |target_url| as a query param. The
  // |loader_url| looks something like:
  // http://127.0.0.1:port_number/prerender/prefetch_loader.html?replace_text=\
  // UkVQTEFDRV9XSVRIX1BSRUZFVENIX1VSTA==:aHR0cDovL3d3dy52dlci5odG1s.
  // When the embedded test server receives the request, it uses the specified
  // query params to replace the "REPLACE_WITH_PREFETCH_URL" string in the HTML
  // response with |target_url|. See method UpdateReplacedText() from embedded
  // test server.
  std::unique_ptr<TestPrerender> PrefetchFromURL(
      const GURL& target_url,
      FinalStatus expected_final_status,
      int expected_number_of_loads = 0) {
    GURL loader_url = ServeLoaderURL(
        kPrefetchLoaderPath, "REPLACE_WITH_PREFETCH_URL", target_url, "");
    std::vector<FinalStatus> expected_final_status_queue(1,
                                                         expected_final_status);
    std::vector<std::unique_ptr<TestPrerender>> prerenders =
        NavigateWithPrerenders(loader_url, expected_final_status_queue);
    prerenders[0]->WaitForLoads(0);

    if (ShouldAbortPrerenderBeforeSwap(expected_final_status_queue.front())) {
      // The prefetcher will abort on its own. Assert it does so correctly.
      prerenders[0]->WaitForStop();
      EXPECT_FALSE(prerenders[0]->contents());
    } else {
      // Otherwise, check that it prefetched correctly.
      test_utils::TestNoStatePrefetchContents* no_state_prefetch_contents =
          prerenders[0]->contents();
      if (no_state_prefetch_contents) {
        EXPECT_EQ(FINAL_STATUS_UNKNOWN,
                  no_state_prefetch_contents->final_status());
      }
    }

    return std::move(prerenders[0]);
  }

  // Returns true if the prerender is expected to abort on its own, before
  // attempting to swap it.
  bool ShouldAbortPrerenderBeforeSwap(FinalStatus status) {
    switch (status) {
      case FINAL_STATUS_USED:
      case FINAL_STATUS_APP_TERMINATING:
      case FINAL_STATUS_PROFILE_DESTROYED:
      case FINAL_STATUS_CACHE_OR_HISTORY_CLEARED:
      // We'll crash the renderer after it's loaded.
      case FINAL_STATUS_RENDERER_CRASHED:
      case FINAL_STATUS_CANCELLED:
        return false;
      default:
        return true;
    }
  }

  std::unique_ptr<TestPrerender> PrefetchFromFile(
      const std::string& html_file,
      FinalStatus expected_final_status,
      int expected_number_of_loads = 0) {
    return PrefetchFromURL(src_server()->GetURL(MakeAbsolute(html_file)),
                           expected_final_status, expected_number_of_loads);
  }

  // Returns length of |no_state_prefetch_manager_|'s history, or SIZE_MAX on
  // failure.
  size_t GetHistoryLength() const {
    base::Value::Dict prerender_dict =
        GetNoStatePrefetchManager()->CopyAsDict();
    if (const base::Value::List* history_list =
            prerender_dict.FindList("history")) {
      return history_list->size();
    }
    return std::numeric_limits<size_t>::max();
  }

  // Clears the specified data using BrowsingDataRemover.
  void ClearBrowsingData(Browser* browser, uint64_t remove_mask) {
    content::BrowsingDataRemover* remover =
        browser->profile()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver observer(remover);
    remover->RemoveAndReply(
        base::Time(), base::Time::Max(), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
    observer.BlockUntilCompletion();
    // BrowsingDataRemover deletes itself.
  }

  // Opens the prerendered page using javascript functions in the loader
  // page. |javascript_function_name| should be a 0 argument function which is
  // invoked. |new_web_contents| is true if the navigation is expected to
  // happen in a new WebContents via OpenURL.
  void OpenURLWithJSImpl(const std::string& javascript_function_name,
                         const GURL& url,
                         const GURL& ping_url,
                         bool new_web_contents) const {
    content::WebContents* web_contents = GetActiveWebContents();
    content::RenderFrameHost* render_frame_host =
        web_contents->GetPrimaryMainFrame();
    // Extra arguments in JS are ignored.
    std::string javascript =
        base::StringPrintf("%s('%s', '%s')", javascript_function_name.c_str(),
                           url.spec().c_str(), ping_url.spec().c_str());

    if (new_web_contents) {
      NewTabNavigationOrSwapObserver observer;
      render_frame_host->ExecuteJavaScriptWithUserGestureForTests(
          base::ASCIIToUTF16(javascript), base::NullCallback(),
          content::ISOLATED_WORLD_ID_GLOBAL);
      observer.Wait();
    } else {
      NavigationOrSwapObserver observer(current_browser()->tab_strip_model(),
                                        web_contents);
      render_frame_host->ExecuteJavaScriptForTests(
          base::ASCIIToUTF16(javascript), base::NullCallback(),
          content::ISOLATED_WORLD_ID_GLOBAL);
      observer.Wait();
    }
  }

  void OpenDestURLViaClickNewWindow(GURL& dest_url) const {
    OpenURLWithJSImpl("ShiftClick", dest_url, GURL(), true);
  }

  void OpenDestURLViaClickNewForegroundTab(GURL& dest_url) const {
#if BUILDFLAG(IS_MAC)
    OpenURLWithJSImpl("MetaShiftClick", dest_url, GURL(), true);
#else
    OpenURLWithJSImpl("CtrlShiftClick", dest_url, GURL(), true);
#endif
  }

  base::SimpleTestTickClock clock_;

 private:
  // Disable sampling of UKM preloading logs.
  content::test::PreloadingConfigOverride preloading_config_override_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      link_rel_attempt_entry_builder_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> test_timer_;
  base::test::ScopedFeatureList feature_list_;
};

class NoStatePrefetchBrowserTestHttpCache
    : public NoStatePrefetchBrowserTest,
      public testing::WithParamInterface<SplitCacheTestCase> {
 protected:
  NoStatePrefetchBrowserTestHttpCache()
      : split_cache_experiment_feature_list_(GetParam(),
                                             kTestCaseToFeatureMapping) {
    if (IsSplitCacheEnabled()) {
      split_cache_enabled_feature_list_.InitAndEnableFeature(
          net::features::kSplitCacheByNetworkIsolationKey);
    } else {
      split_cache_enabled_feature_list_.InitAndDisableFeature(
          net::features::kSplitCacheByNetworkIsolationKey);
    }
  }

  bool IsSplitCacheEnabled() const {
    return GetParam() != SplitCacheTestCase::kDisabled;
  }

 private:
  net::test::ScopedMutuallyExclusiveFeatureList
      split_cache_experiment_feature_list_;
  base::test::ScopedFeatureList split_cache_enabled_feature_list_;
};

class NoStatePrefetchBrowserTestHttpCacheDefaultAndAppendFrameOrigin
    : public NoStatePrefetchBrowserTest {
 protected:
  NoStatePrefetchBrowserTestHttpCacheDefaultAndAppendFrameOrigin() {
    feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that the network isolation key is correctly populated during a prefetch.
IN_PROC_BROWSER_TEST_F(
    NoStatePrefetchBrowserTestHttpCacheDefaultAndAppendFrameOrigin,
    PrefetchTwoCrossOriginFrames) {
  GURL image_src =
      embedded_test_server()->GetURL("/prerender/cacheable_image.png");
  base::StringPairs replacement_text_img_src;
  replacement_text_img_src.push_back(
      std::make_pair("IMAGE_SRC", image_src.spec()));
  std::string iframe_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/one_image.html", replacement_text_img_src);

  GURL iframe_src_1 = embedded_test_server()->GetURL("www.a.com", iframe_path);
  GURL iframe_src_2 = embedded_test_server()->GetURL("www.b.com", iframe_path);

  base::StringPairs replacement_text_iframe_src;
  replacement_text_iframe_src.push_back(
      std::make_pair("IFRAME_1_SRC", iframe_src_1.spec()));
  replacement_text_iframe_src.push_back(
      std::make_pair("IFRAME_2_SRC", iframe_src_2.spec()));
  std::string prerender_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/two_iframes.html", replacement_text_iframe_src);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(prerender_path)));

  WaitForRequestCount(image_src, 2);
}

// Checks that a page is correctly prefetched in the case of a
// <link rel=prerender> tag and the JavaScript on the page is not executed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchSimple) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(kPrefetchPage)));
  {
    // Check that we store one entry corresponding to NoStatePrefetch attempt.
    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    // NoStatePrefetch should be successful with status kReady.
    std::vector<UkmEntry> expected_attempt_entries = {
        link_rel_attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kNoStatePrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kReady,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/
            base::ScopedMockElapsedTimersForTest::kMockElapsedTime),
    };
    EXPECT_THAT(attempt_ukm_entries,
                testing::UnorderedElementsAreArray(expected_attempt_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(
               attempt_ukm_entries, expected_attempt_entries);
  }
}

// Checks that prefetching is not stopped forever by aggressive background load
// limits.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchBigger) {
  std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
      kPrefetchPageBigger, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
  // The |kPrefetchPng| is requested twice because the |kPrefetchPngRedirect|
  // redirects to it.
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 2);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPngRedirect), 1);
}

using NoStatePrefetchBrowserTestHttpCacheDefaultAndDoubleKeyed =
    NoStatePrefetchBrowserTestHttpCache;

// Checks that a page load following a prefetch reuses preload-scanned resources
// and link rel 'prerender' main resource from cache without failing over to
// network.
IN_PROC_BROWSER_TEST_P(NoStatePrefetchBrowserTestHttpCacheDefaultAndDoubleKeyed,
                       LoadAfterPrefetch) {
  {
    std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
        kPrefetchPageBigger, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
    WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
    WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
    WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);
  }
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(kPrefetchPageBigger)));
  // Check that the request counts did not increase.
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);
}

// Checks that a page load following a cross origin prefetch reuses
// preload-scanned resources and link rel 'prerender' main resource
// from cache without failing over to network.
IN_PROC_BROWSER_TEST_P(NoStatePrefetchBrowserTestHttpCacheDefaultAndDoubleKeyed,
                       LoadAfterPrefetchCrossOrigin) {
  GURL cross_domain_url = embedded_test_server()->GetURL(
      test_utils::kSecondaryDomain, kPrefetchPageBigger);

  PrefetchFromURL(cross_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(current_browser(), cross_domain_url));
  size_t expected_navigation_request_count;
  // For schemes that partition main-frame navigations separately from resource
  // loads, we'll expect the browser-initiated navigation to result in a cache
  // miss but we expect caching of the other resources.
  switch (GetParam()) {
    case SplitCacheTestCase::kDisabled:
    case SplitCacheTestCase::kEnabledTripleKeyed:
      expected_navigation_request_count = 1;
      break;
    case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
    case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
    case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
      expected_navigation_request_count = 2;
      break;
  }
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger),
                      expected_navigation_request_count);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);
}

IN_PROC_BROWSER_TEST_P(NoStatePrefetchBrowserTestHttpCacheDefaultAndDoubleKeyed,
                       LoadAfterPrefetchCrossOriginRendererInitiated) {
  static const std::string kSecondaryDomain = "www.foo.com";
  GURL cross_domain_url =
      embedded_test_server()->GetURL(kSecondaryDomain, kPrefetchPageBigger);

  PrefetchFromURL(cross_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);

  // Navigate to a page with the same origin as the one used for prefetching.
  // This makes it so that the renderer-initiated navigation below is keyed
  // using the same initiator (for HTTP cache experiment partitioning schemes
  // that key on initiator).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL("/empty.html")));

  ASSERT_TRUE(NavigateToURLFromRenderer(current_browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetPrimaryMainFrame(),
                                        cross_domain_url));

  WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NoStatePrefetchBrowserTestHttpCacheDefaultAndDoubleKeyed,
    testing::ValuesIn(
        {SplitCacheTestCase::kDisabled, SplitCacheTestCase::kEnabledTripleKeyed,
         SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
         SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
         SplitCacheTestCase::kEnabledTriplePlusNavInitiator}),
    [](const testing::TestParamInfo<SplitCacheTestCase>& info) {
      switch (info.param) {
        case (SplitCacheTestCase::kDisabled):
          return "SplitCacheDisabled";
        case (SplitCacheTestCase::kEnabledTripleKeyed):
          return "TripleKeyed";
        case (SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool):
          return "TriplePlusCrossSiteMainFrameNavigationBool";
        case (SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator):
          return "TriplePlusMainFrameNavigationInitiator";
        case (SplitCacheTestCase::kEnabledTriplePlusNavInitiator):
          return "TriplePlusNavigationInitiator";
      }
    });

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchAllResourceTypes) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 1);
}

// Test and Test Class for lightweight prefetch under the HTML configuration.
class HTMLOnlyNoStatePrefetchBrowserTest : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["html_only"] = "true";
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLightweightNoStatePrefetch, parameters}}, {});
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(HTMLOnlyNoStatePrefetchBrowserTest, PrefetchHTMLOnly) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 0);
}

// Test and Test Class for lightweight prefetch under the HTML+CSS
// configuration.
class HTMLCSSNoStatePrefetchBrowserTest : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["skip_script"] = "true";
    parameters["skip_other"] = "true";
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLightweightNoStatePrefetch, parameters}}, {});
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(HTMLCSSNoStatePrefetchBrowserTest, PrefetchHTMLCSS) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 0);
}

// Test and Test Class for lightweight prefetch under the HTML+CSS+SyncScript
// configuration.
class HTMLCSSSyncScriptNoStatePrefetchBrowserTest
    : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["skip_other"] = "true";
    parameters["skip_async_script"] = "true";
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLightweightNoStatePrefetch, parameters}}, {});
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(HTMLCSSSyncScriptNoStatePrefetchBrowserTest,
                       PrefetchHTMLCSSSyncScript) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 0);
}

// Test and Test Class for lightweight prefetch under the
// HTML+CSS+SyncScript+Font configuration.
class HTMLCSSSyncScriptFontNoStatePrefetchBrowserTest
    : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["skip_other"] = "true";
    parameters["skip_async_script"] = "true";
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLightweightNoStatePrefetch, parameters}}, {});
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(HTMLCSSSyncScriptFontNoStatePrefetchBrowserTest,
                       PrefetchHTMLCSSSyncScript) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 0);
}

// Test and Test Class for lightweight prefetch under the HTML+CSS+Script
// configuration.
class HTMLCSSScriptNoStatePrefetchBrowserTest
    : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["skip_other"] = "true";
    parameters["skip_async_script"] = "false";
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLightweightNoStatePrefetch, parameters}}, {});
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(HTMLCSSScriptNoStatePrefetchBrowserTest,
                       PrefetchHTMLCSSScript) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 0);
}

void GetCookieCallback(base::RepeatingClosure callback,
                       const net::CookieAccessResultList& cookie_list,
                       const net::CookieAccessResultList& excluded_cookies) {
  bool found_chocolate = false;
  bool found_oatmeal = false;
  for (const auto& c : cookie_list) {
    if (c.cookie.Name() == "chocolate-chip") {
      EXPECT_EQ("the-best", c.cookie.Value());
      found_chocolate = true;
    }
    if (c.cookie.Name() == "oatmeal") {
      EXPECT_EQ("sublime", c.cookie.Value());
      found_oatmeal = true;
    }
  }
  CHECK(found_chocolate && found_oatmeal);
  callback.Run();
}

// Check cookie loading for prefetched pages.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchCookie) {
  GURL url = src_server()->GetURL(kPrefetchCookiePage);
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromURL(url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  content::StoragePartition* storage_partition =
      current_browser()->profile()->GetStoragePartitionForUrl(url, false);
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  base::RunLoop loop;
  storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      url, options, net::CookiePartitionKeyCollection(),
      base::BindOnce(GetCookieCallback, loop.QuitClosure()));
  loop.Run();
}

// Check cookie loading for a cross-domain prefetched pages.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchCookieCrossDomain) {
  GURL cross_domain_url(base::StringPrintf(
      "http://%s:%d%s", test_utils::kSecondaryDomain,
      embedded_test_server()->host_port_pair().port(), kPrefetchCookiePage));

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromURL(cross_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // While the request is cross-site, it's permitted to set (implicitly) lax
  // cookies on a cross-site navigation.
  content::StoragePartition* storage_partition =
      current_browser()->profile()->GetStoragePartitionForUrl(cross_domain_url,
                                                              false);
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  base::RunLoop loop;
  storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      cross_domain_url, options, net::CookiePartitionKeyCollection(),
      base::BindOnce(GetCookieCallback, loop.QuitClosure()));
  loop.Run();
}

// Check cookie loading for a cross-domain prefetched pages.
IN_PROC_BROWSER_TEST_P(NoStatePrefetchBrowserTestHttpCacheDefaultAndDoubleKeyed,
                       PrefetchCookieCrossDomainSameSiteStrict) {
  UseHttpsSrcServer();
  GURL cross_domain_url =
      src_server()->GetURL(test_utils::kSecondaryDomain, "/echoall");

  EXPECT_TRUE(SetCookie(current_browser()->profile(), cross_domain_url,
                        "cookie_A=A; SameSite=Strict;"));
  EXPECT_TRUE(SetCookie(current_browser()->profile(), cross_domain_url,
                        "cookie_B=B; SameSite=Lax;"));

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromURL(cross_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(current_browser(), cross_domain_url));

  EXPECT_TRUE(WaitForLoadStop(
      current_browser()->tab_strip_model()->GetActiveWebContents()));

  std::string html_content =
      content::EvalJs(
          current_browser()->tab_strip_model()->GetActiveWebContents(),
          "document.body.innerHTML")
          .ExtractString();

  // The prerender request will be considered a renderer-inititated cross-origin
  // navigation, so the SameSite=Strict cookie should not be sent and the
  // SameSite=Lax cookie should be. Note that we can tell whether the prerender
  // response is actually used here (assuming the prerender code is working
  // correctly) because if it isn't, `ui_test_utils::NavigateToURL()` will
  // perform a browser-initiated navigation which will cause SameSite=Strict
  // cookies to be sent.
  switch (GetParam()) {
    case SplitCacheTestCase::kDisabled:
    case SplitCacheTestCase::kEnabledTripleKeyed:
      EXPECT_EQ(std::string::npos, html_content.find("cookie_A=A"));
      break;
    case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
    case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
    case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
      // For schemes that partition cross-site renderer-initiated navigations
      // separately from browser-initiated navigations, we'll expect the latter
      // to result in a cache miss.
      EXPECT_NE(std::string::npos, html_content.find("cookie_A=A"));
      break;
  }
  EXPECT_NE(std::string::npos, html_content.find("cookie_B=B"));
}

// Check cookie loading for a same-domain prefetched pages.
IN_PROC_BROWSER_TEST_P(NoStatePrefetchBrowserTestHttpCacheDefaultAndDoubleKeyed,
                       PrefetchCookieSameDomainSameSiteStrict) {
  UseHttpsSrcServer();
  GURL same_domain_url = src_server()->GetURL("/echoall");

  EXPECT_TRUE(SetCookie(current_browser()->profile(), same_domain_url,
                        "cookie_A=A; SameSite=Strict;"));
  EXPECT_TRUE(SetCookie(current_browser()->profile(), same_domain_url,
                        "cookie_B=B; SameSite=Lax;"));

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromURL(same_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Modify the stored SameSite=Strict cookie so that we can tell whether the
  // prerendered response is used by the navigation below.
  EXPECT_TRUE(SetCookie(current_browser()->profile(), same_domain_url,
                        "cookie_A=Modified; SameSite=Strict;"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(current_browser(), same_domain_url));

  EXPECT_TRUE(WaitForLoadStop(
      current_browser()->tab_strip_model()->GetActiveWebContents()));

  std::string html_content =
      content::EvalJs(
          current_browser()->tab_strip_model()->GetActiveWebContents(),
          "document.body.innerHTML")
          .ExtractString();

  // Since the prerender request is a renderer-initiated same-origin navigation,
  // SameSite=Strict cookies should be sent (the same way that they will be sent
  // for browser-initiated navigations).
  EXPECT_NE(std::string::npos, html_content.find("cookie_A=A"))
      << "html_content: " << html_content;
  EXPECT_NE(std::string::npos, html_content.find("cookie_B=B"));
}

// Check that the LOAD_PREFETCH flag is set.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchLoadFlag) {
  GURL prefetch_page = src_server()->GetURL(kPrefetchPage);
  GURL prefetch_script = src_server()->GetURL(kPrefetchScript);

  content::URLLoaderMonitor monitor({prefetch_page, prefetch_script});

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(prefetch_page, 1);
  WaitForRequestCount(prefetch_script, 1);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  monitor.WaitForUrls();

  std::optional<network::ResourceRequest> page_request =
      monitor.GetRequestInfo(prefetch_page);
  EXPECT_TRUE(page_request->load_flags & net::LOAD_PREFETCH);
  std::optional<network::ResourceRequest> script_request =
      monitor.GetRequestInfo(prefetch_script);
  EXPECT_TRUE(script_request->load_flags & net::LOAD_PREFETCH);
}

// Check that prefetched resources and subresources set the 'Purpose: prefetch'
// header.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PurposeHeaderIsSet) {
  GURL prefetch_page = src_server()->GetURL(kPrefetchPage);
  GURL prefetch_script = src_server()->GetURL(kPrefetchScript);

  content::URLLoaderMonitor monitor({prefetch_page, prefetch_script});

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(prefetch_page, 1);
  WaitForRequestCount(prefetch_script, 1);
  monitor.WaitForUrls();
  for (const GURL& url : {prefetch_page, prefetch_script}) {
    std::optional<network::ResourceRequest> request =
        monitor.GetRequestInfo(url);
    EXPECT_TRUE(request->load_flags & net::LOAD_PREFETCH);
    EXPECT_FALSE(request->headers.HasHeader(kExpectedPurposeHeaderOnPrefetch));
    EXPECT_TRUE(request->cors_exempt_headers.HasHeader(
        kExpectedPurposeHeaderOnPrefetch));
    EXPECT_EQ("prefetch", request->cors_exempt_headers
                              .GetHeader(kExpectedPurposeHeaderOnPrefetch)
                              .value_or(std::string()));
  }
}

// Check that on normal navigations the 'Purpose: prefetch' header is not set.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PurposeHeaderNotSetWhenNotPrefetching) {
  GURL prefetch_page = src_server()->GetURL(kPrefetchPage);
  GURL prefetch_script = src_server()->GetURL(kPrefetchScript);
  GURL prefetch_script2 = src_server()->GetURL(kPrefetchScript2);

  content::URLLoaderMonitor monitor(
      {prefetch_page, prefetch_script, prefetch_script2});

  ASSERT_TRUE(ui_test_utils::NavigateToURL(current_browser(), prefetch_page));
  WaitForRequestCount(prefetch_page, 1);
  WaitForRequestCount(prefetch_script, 1);
  WaitForRequestCount(prefetch_script2, 1);
  monitor.WaitForUrls();
  for (const GURL& url : {prefetch_page, prefetch_script, prefetch_script2}) {
    std::optional<network::ResourceRequest> request =
        monitor.GetRequestInfo(url);
    EXPECT_FALSE(request->load_flags & net::LOAD_PREFETCH);
    EXPECT_FALSE(request->headers.HasHeader(kExpectedPurposeHeaderOnPrefetch));
    EXPECT_FALSE(request->cors_exempt_headers.HasHeader(
        kExpectedPurposeHeaderOnPrefetch));
  }
}

// Checks the prefetch of an img tag.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchImage) {
  GURL main_page_url = GetURLWithReplacement(
      kPrefetchImagePage, "REPLACE_WITH_IMAGE_URL", kPrefetchJpeg);
  PrefetchFromURL(main_page_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
}

// Checks that a cross-domain prefetching works correctly.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchCrossDomain) {
  GURL cross_domain_url(base::StringPrintf(
      "http://%s:%d%s", test_utils::kSecondaryDomain,
      embedded_test_server()->host_port_pair().port(), kPrefetchPage));
  PrefetchFromURL(cross_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
}

// Checks that prefetching from a cross-domain subframe works correctly.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrefetchFromCrossDomainSubframe) {
  GURL target_url(base::StringPrintf(
      "http://%s:%d%s", test_utils::kSecondaryDomain,
      embedded_test_server()->host_port_pair().port(), kPrefetchPage));

  GURL inner_frame_url = ServeLoaderURLWithHostname(
      kPrefetchLoaderPath, "REPLACE_WITH_PREFETCH_URL", target_url, "",
      test_utils::kSecondaryDomain);

  GURL outer_frame_url = ServeLoaderURL(
      kPrefetchFromSubframe, "REPLACE_WITH_SUBFRAME_URL", inner_frame_url, "");

  std::vector<FinalStatus> expected_final_status_queue(
      1, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  std::vector<std::unique_ptr<TestPrerender>> prerenders =
      NavigateWithPrerenders(outer_frame_url, expected_final_status_queue);
  prerenders[0]->WaitForStop();

  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
}

// Checks that response header CSP is respected.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, ResponseHeaderCSP) {
  GURL second_script_url(base::StringPrintf(
      "http://%s%s", test_utils::kSecondaryDomain, kPrefetchScript2));
  GURL prefetch_response_header_csp = GetURLWithReplacement(
      kPrefetchResponseHeaderCSP, "REPLACE_WITH_PORT",
      base::NumberToString(src_server()->host_port_pair().port()));

  PrefetchFromURL(prefetch_response_header_csp,
                  FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // The second script is in the correct domain for CSP, but the first script is
  // not.
  WaitForRequestCount(prefetch_response_header_csp, 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
}

// Checks that CSP in the meta tag cancels the prefetch.
// TODO(mattcary): probably this behavior should be consistent with
// response-header CSP. See crbug/656581.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, MetaTagCSP) {
  GURL second_script_url(base::StringPrintf(
      "http://%s%s", test_utils::kSecondaryDomain, kPrefetchScript2));
  GURL prefetch_meta_tag_csp = GetURLWithReplacement(
      kPrefetchMetaCSP, "REPLACE_WITH_PORT",
      base::NumberToString(src_server()->host_port_pair().port()));

  PrefetchFromURL(prefetch_meta_tag_csp,
                  FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // TODO(mattcary): See test comment above. If the meta CSP tag were parsed,
  // |second_script| would be loaded. Instead as the background scanner bails as
  // soon as the meta CSP tag is seen, only |main_page| is fetched.
  WaitForRequestCount(prefetch_meta_tag_csp, 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
}

// Checks that the second prefetch request succeeds. This test waits for
// Prerender Stop before starting the second request.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchMultipleRequest) {
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  PrefetchFromFile(kPrefetchPage2, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
}

// Checks that a second prefetch request, started before the first stops,
// succeeds.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchSimultaneous) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetActiveWebContents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  GURL first_url = src_server()->GetURL("/hung");

  // Start the first prefetch directly instead of via PrefetchFromFile for the
  // first prefetch to avoid the wait on prerender stop.
  GURL first_loader_url = ServeLoaderURL(
      kPrefetchLoaderPath, "REPLACE_WITH_PREFETCH_URL", first_url, "");
  std::vector<FinalStatus> first_expected_status_queue(1,
                                                       FINAL_STATUS_CANCELLED);
  NavigateWithPrerenders(first_loader_url, first_expected_status_queue);

  PrefetchFromFile(kPrefetchPage2, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);

  {
    // Check that we store one entry corresponding to the first NoStatePrefetch
    // attempt. Since we don't navigate again, we don't store another no state
    // prefetch attempt.
    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    // NoStatePrefetch status for first attempt should be equal to kRunning as,
    // we start the second prefetch without finishing the first NSP.
    std::vector<UkmEntry> expected_attempt_entries = {
        link_rel_attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kNoStatePrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kRunning,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
    };
    EXPECT_THAT(attempt_ukm_entries,
                testing::UnorderedElementsAreArray(expected_attempt_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(
               attempt_ukm_entries, expected_attempt_entries);
  }
}

// Checks that a prefetch does not recursively prefetch.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, NoPrefetchRecursive) {
  // A prefetch of a page with a prefetch of the image page should not load the
  // image page.
  PrefetchFromFile(kPrefetchRecursePage,
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchRecursePage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchNostorePage), 0);

  // When the first page is loaded, the image page should be prefetched. The
  // test may finish before the prefetcher is torn down, so
  // IgnoreNoStatePrefetchContents() is called to skip the final status check.
  no_state_prefetch_contents_factory()->IgnoreNoStatePrefetchContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(kPrefetchRecursePage)));
  WaitForRequestCount(src_server()->GetURL(kPrefetchNostorePage), 1);
}

// Checks a prefetch to a nonexisting page.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchNonexisting) {
  std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
      "/nonexisting-page.html", FINAL_STATUS_UNSUPPORTED_SCHEME);
}

// Checks that a 301 redirect is followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301Redirect) {
  PrefetchFromFile(CreateServerRedirect(kPrefetchPage),
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// Checks that non-HTTP(S) main resource redirects are marked as unsupported
// scheme.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrefetchRedirectUnsupportedScheme) {
  PrefetchFromFile(
      CreateServerRedirect("invalidscheme://www.google.com/test.html"),
      FINAL_STATUS_UNSUPPORTED_SCHEME, 1);
}

// Checks that a 302 redirect is followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch302Redirect) {
  PrefetchFromFile(k302RedirectPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// Checks that the load flags are set correctly for all resources in a 301
// redirect chain.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301LoadFlags) {
  std::string redirect_path = CreateServerRedirect(kPrefetchPage);
  GURL redirect_url = src_server()->GetURL(redirect_path);
  GURL page_url = src_server()->GetURL(kPrefetchPage);
  content::URLLoaderMonitor monitor({redirect_url});

  PrefetchFromFile(redirect_path, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(redirect_url, 1);
  WaitForRequestCount(page_url, 1);
  monitor.WaitForUrls();

  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(redirect_url);
  EXPECT_TRUE(request->load_flags & net::LOAD_PREFETCH);
}

// Checks that a subresource 301 redirect is followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301Subresource) {
  PrefetchFromFile(kPrefetchSubresourceRedirectPage,
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// Checks a client redirect is not followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchClientRedirect) {
  PrefetchFromFile(
      "/client-redirect/?" + base::EscapeQueryParamValue(kPrefetchPage, false),
      FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(kPrefetchPage2)));
  // A complete load of kPrefetchPage2 is used as a sentinel. Otherwise the test
  // ends before script_counter would reliably see the load of kPrefetchScript,
  // were it to happen.
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
}

// Prefetches a page that contains an automatic download triggered through an
// iframe. The request to download should not reach the server.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchDownloadIframe) {
  PrefetchFromFile("/prerender/prerender_download_iframe.html",
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // A complete load of kPrefetchPage2 is used as a sentinel as in test
  // |PrefetchClientRedirect| above.
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchDownloadFile), 0);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrefetchDownloadViaClientRedirect) {
  PrefetchFromFile("/prerender/prerender_download_refresh.html",
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // A complete load of kPrefetchPage2 is used as a sentinel as in test
  // |PrefetchClientRedirect| above.
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchDownloadFile), 0);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchPageWithFragment) {
  std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
      kPrefetchPageWithFragment, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
}

// Checks that a prefetch of a CRX will result in a cancellation due to
// download.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchCrx) {
  PrefetchFromFile("/prerender/extension.crx", FINAL_STATUS_DOWNLOAD);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchHttps) {
  UseHttpsSrcServer();
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// Checks that an SSL error prevents prefetch.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, SSLError) {
  // Only send the loaded page, not the loader, through SSL.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  std::unique_ptr<TestPrerender> prerender = PrefetchFromURL(
      https_server.GetURL(kPrefetchPage), FINAL_STATUS_SSL_ERROR);
  DestructionWaiter waiter(prerender->contents(), FINAL_STATUS_SSL_ERROR);
  EXPECT_TRUE(waiter.WaitForDestroy());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(kPrefetchPage)));
  {
    // Check that we store one entry corresponding to NoStatePrefetch attempt.
    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    // NoStatePrefetch should fail with SSLError.
    std::vector<UkmEntry> expected_attempt_entries = {
        link_rel_attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kNoStatePrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kFailure,
            ToPreloadingFailureReasonFromFinalStatus(FINAL_STATUS_SSL_ERROR),
            /*accurate=*/false),
    };
    EXPECT_THAT(attempt_ukm_entries,
                testing::UnorderedElementsAreArray(expected_attempt_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(
               attempt_ukm_entries, expected_attempt_entries);
  }
}

// Checks that a subresource failing SSL does not prevent prefetch on the rest
// of the page.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, SSLSubresourceError) {
  // First confirm that the image loads as expected.

  // A separate HTTPS server is started for the subresource; src_server() is
  // non-HTTPS.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("/prerender/image.jpeg");
  GURL main_page_url = GetURLWithReplacement(
      kPrefetchImagePage, "REPLACE_WITH_IMAGE_URL", https_url.spec());

  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromURL(main_page_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // Checks that the presumed failure of the image load didn't affect the script
  // fetch. This assumes waiting for the script load is enough to see any error
  // from the image load.
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Loop) {
  std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
      kPrefetchLoopPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchLoopPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// Crashes on Win.  http://crbug.com/1516892
#if BUILDFLAG(IS_WIN)
#define MAYBE_RendererCrash DISABLED_RendererCrash
#else
#define MAYBE_RendererCrash RendererCrash
#endif
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, MAYBE_RendererCrash) {
  // Navigate to about:blank to get the session storage namespace.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(current_browser(),
                                           GURL(url::kAboutBlankURL)));
  content::SessionStorageNamespace* storage_namespace =
      GetActiveWebContents()
          ->GetController()
          .GetDefaultSessionStorageNamespace();

  // Navigate to about:crash without an intermediate loader because chrome://
  // URLs are ignored in renderers, and the test server has no support for them.
  GURL url(blink::kChromeUICrashURL);
  const gfx::Size kSize(640, 480);
  std::unique_ptr<TestPrerender> test_prerender =
      no_state_prefetch_contents_factory()->ExpectNoStatePrefetchContents(
          FINAL_STATUS_RENDERER_CRASHED);
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  std::unique_ptr<NoStatePrefetchHandle> no_state_prefetch_handle(
      GetNoStatePrefetchManager()->AddSameOriginSpeculation(
          url, storage_namespace, kSize, url::Origin::Create(url)));
  ASSERT_EQ(no_state_prefetch_handle->contents(), test_prerender->contents());
  test_prerender->WaitForStop();
}

// Checks that the prefetch of png correctly loads the png.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Png) {
  PrefetchFromFile(kPrefetchPng, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 1);
}

// Checks that the prefetch of jpeg correctly loads the jpeg.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Jpeg) {
  PrefetchFromFile(kPrefetchJpeg, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
}

// If the main resource is unsafe, the whole prefetch is cancelled.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrerenderSafeBrowsingTopLevel) {
  GURL url = src_server()->GetURL(kPrefetchPage);
  GetFakeSafeBrowsingDatabaseManager()->AddDangerousUrl(
      url, safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE);

  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_SAFE_BROWSING);

  // The frame request may have been started, but SafeBrowsing must have already
  // blocked it. Verify that the page load did not happen.
  prerender->WaitForLoads(0);

  // The frame resource has been blocked by SafeBrowsing, the subresource on
  // the page shouldn't be requested at all.
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
}

// Ensures that server redirects to a malware page will cancel prerenders.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, ServerRedirect) {
  GURL url = src_server()->GetURL("/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->AddDangerousUrl(
      url, safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  PrefetchFromURL(src_server()->GetURL(
                      CreateServerRedirect("/prerender/prerender_page.html")),
                  FINAL_STATUS_SAFE_BROWSING, 0);
}

// Checks that prefetching a page does not add it to browsing history.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, HistoryUntouchedByPrefetch) {
  // Initialize.
  Profile* profile = current_browser()->profile();
  ASSERT_TRUE(profile);
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));

  // Prefetch a page.
  GURL prefetched_url = src_server()->GetURL(kPrefetchPage);
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForHistoryBackendToRun(profile);

  // Navigate to another page.
  GURL navigated_url = src_server()->GetURL(kPrefetchPage2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(current_browser(), navigated_url));
  WaitForHistoryBackendToRun(profile);

  // Check that the URL that was explicitly navigated to is already in history.
  ui_test_utils::HistoryEnumerator enumerator(profile);
  std::vector<GURL>& urls = enumerator.urls();
  EXPECT_TRUE(base::Contains(urls, navigated_url));

  // Check that the URL that was prefetched is not in history.
  EXPECT_FALSE(base::Contains(urls, prefetched_url));

  // The loader URL is the remaining entry.
  EXPECT_EQ(2U, urls.size());
}

// Checks that prefetch requests have net::IDLE priority.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, IssuesIdlePriorityRequests) {
  // TODO(pasko): Figure out how to test that while a prefetched URL is in IDLE
  // priority state, a high-priority request with the same URL from a foreground
  // navigation hits the server.
  GURL script_url = src_server()->GetURL(kPrefetchScript);
  content::URLLoaderMonitor monitor({script_url});

  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(script_url, 1);
  monitor.WaitForUrls();

#if BUILDFLAG(IS_ANDROID)
  // On Android requests from prerenders do not get downgraded
  // priority. See: https://crbug.com/652746.
  constexpr net::RequestPriority kExpectedPriority = net::HIGHEST;
#else
  constexpr net::RequestPriority kExpectedPriority = net::IDLE;
#endif
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(script_url);
  EXPECT_EQ(kExpectedPriority, request->priority);
}

// Checks that a registered ServiceWorker (SW) that is not currently running
// will intercepts a prefetch request.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, ServiceWorkerIntercept) {
  // Register and launch a SW.
  std::u16string expected_title = u"SW READY";
  content::TitleWatcher title_watcher(GetActiveWebContents(), expected_title);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(kServiceWorkerLoader)));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Stop any SW, killing the render process in order to test that the
  // lightweight renderer created for NoState prefetch does not interfere with
  // SW startup.
  int host_count = 0;
  for (content::RenderProcessHost::iterator iter(
           content::RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    // Don't count spare RenderProcessHosts.
    if (!iter.GetCurrentValue()->HostHasNotBeenUsed())
      ++host_count;

    content::RenderProcessHostWatcher process_exit_observer(
        iter.GetCurrentValue(),
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    // TODO(wez): This used to use wait=true.
    iter.GetCurrentValue()->Shutdown(content::RESULT_CODE_KILLED);
    process_exit_observer.Wait();
  }
  // There should be at most one render_process_host, that created for the SW.
  EXPECT_EQ(1, host_count);

  // Open a new tab to replace the one closed with all the RenderProcessHosts.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The SW intercepts kPrefetchPage and replaces it with a body that contains
  // an <img> tage for kPrefetchPng. This verifies that the SW ran correctly by
  // observing the fetch of the image.
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 1);
}

class NoStatePrefetchIncognitoBrowserTest : public NoStatePrefetchBrowserTest {
 public:
  void SetUpOnMainThread() override {
    Profile* normal_profile = current_browser()->profile();
    set_browser(OpenURLOffTheRecord(normal_profile, GURL("about:blank")));
    NoStatePrefetchBrowserTest::SetUpOnMainThread();
    current_browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(content_settings::CookieControlsMode::kOff));
  }
};

// Checks that prerendering works in incognito mode.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchIncognitoBrowserTest,
                       PrerenderIncognito) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
}

// Checks that prerenders are aborted when an incognito profile is closed.
// TODO(crbug.com/41476151): The test is crashing on multiple platforms.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchIncognitoBrowserTest,
                       DISABLED_PrerenderIncognitoClosed) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kHungPrerenderPage, FINAL_STATUS_PROFILE_DESTROYED);
  current_browser()->window()->Close();
  test_prerender->WaitForStop();
}

// Checks that when the history is cleared, NoStatePrefetch history is cleared.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, ClearHistory) {
  std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
      kHungPrerenderPage, FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);

  ClearBrowsingData(current_browser(),
                    chrome_browsing_data_remover::DATA_TYPE_HISTORY);
  test_prerender->WaitForStop();

  // Make sure prerender history was cleared.
  EXPECT_EQ(0U, GetHistoryLength());
}

// Checks that when the cache is cleared, NoStatePrefetch history is not
// cleared.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, ClearCache) {
  std::unique_ptr<TestPrerender> prerender = PrefetchFromFile(
      kHungPrerenderPage, FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);

  ClearBrowsingData(current_browser(),
                    content::BrowsingDataRemover::DATA_TYPE_CACHE);
  prerender->WaitForStop();

  // Make sure prerender history was not cleared.  Not a vital behavior, but
  // used to compare with ClearHistory test.
  EXPECT_EQ(1U, GetHistoryLength());
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, CancelAll) {
  GURL url = src_server()->GetURL(kHungPrerenderPage);
  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromURL(url, FINAL_STATUS_CANCELLED, 0);

  GetNoStatePrefetchManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  EXPECT_FALSE(prerender->contents());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(kPrefetchPage)));
  {
    // Check that we store one entry corresponding to NoStatePrefetch attempt.
    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    // NoStatePrefetch should fail with canceled reason.
    std::vector<UkmEntry> expected_attempt_entries = {
        link_rel_attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kNoStatePrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kFailure,
            ToPreloadingFailureReasonFromFinalStatus(FINAL_STATUS_CANCELLED),
            /*accurate=*/false),
    };
    EXPECT_THAT(attempt_ukm_entries,
                testing::UnorderedElementsAreArray(expected_attempt_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(
               attempt_ukm_entries, expected_attempt_entries);
  }
}

// Cancels the prerender of a page with its own prerender.  The second prerender
// should never be started.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       CancelPrerenderWithPrerender) {
  GURL url = src_server()->GetURL("/prerender/prerender_infinite_a.html");

  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromURL(url, FINAL_STATUS_CANCELLED);

  GetNoStatePrefetchManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  EXPECT_FALSE(prerender->contents());
}

// Checks shutdown code while a prerender is active.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrerenderQuickQuit) {
  GURL url = src_server()->GetURL(kHungPrerenderPage);
  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromURL(url, FINAL_STATUS_APP_TERMINATING);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrerenderClickNewWindow) {
  GURL url = src_server()->GetURL("/prerender/prerender_page_with_link.html");
  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromURL(url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  OpenDestURLViaClickNewWindow(url);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrerenderClickNewForegroundTab) {
  GURL url = src_server()->GetURL("/prerender/prerender_page_with_link.html");
  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromURL(url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  OpenDestURLViaClickNewForegroundTab(url);
}

// Checks that renderers using excessive memory will be terminated.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrerenderExcessiveMemory) {
  ASSERT_TRUE(GetNoStatePrefetchManager());
  GetNoStatePrefetchManager()->mutable_config().max_bytes = 100;
  PrefetchFromURL(
      src_server()->GetURL("/prerender/prerender_excessive_memory.html"),
      FINAL_STATUS_MEMORY_LIMIT_EXCEEDED);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(kPrefetchPage)));
  {
    // Check that we store one entry corresponding to NoStatePrefetch attempt.
    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    // NoStatePrefetch should fail with memory limit exceeded.
    std::vector<UkmEntry> expected_attempt_entries = {
        link_rel_attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kNoStatePrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kFailure,
            ToPreloadingFailureReasonFromFinalStatus(
                FINAL_STATUS_MEMORY_LIMIT_EXCEEDED),
            /*accurate=*/false),
    };
    EXPECT_THAT(attempt_ukm_entries,
                testing::UnorderedElementsAreArray(expected_attempt_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(
               attempt_ukm_entries, expected_attempt_entries);
  }
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, OpenTaskManager) {
  const std::u16string any_tab = MatchTaskManagerTab("*");
  const std::u16string original = MatchTaskManagerTab("Prefetch Loader");
  const std::u16string prefetch_page = MatchTaskManagerTab("Prefetch Page");

  // Show the task manager. This populates the model.
  chrome::OpenTaskManager(current_browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, prefetch_page));

  // Prerender a page in addition to the original tab.
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));

  // Open a new tab to replace the one closed with all the RenderProcessHosts.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), src_server()->GetURL(kPrefetchPage),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, prefetch_page));
}

// Renders a page that contains a prerender link to a page that contains an
// img with a source that requires http authentication. This should not
// prerender successfully.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrerenderHttpAuthentication) {
  GURL url =
      src_server()->GetURL("/prerender/prerender_http_auth_container.html");
  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromURL(url, FINAL_STATUS_AUTH_NEEDED);
}

// Checks that the referrer is not set when prerendering and the source page is
// HTTPS.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrerenderNoSSLReferrer) {
  // Use http:// url for the prerendered page main resource.
  GURL url(
      embedded_test_server()->GetURL("/prerender/prerender_no_referrer.html"));

  // Use https:// for all resources.
  UseHttpsSrcServer();

  PrefetchFromURL(url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(current_browser(), url));
  EXPECT_TRUE(WaitForLoadStop(
      current_browser()->tab_strip_model()->GetActiveWebContents()));
  content::WebContents* web_contents =
      current_browser()->tab_strip_model()->GetActiveWebContents();
  const std::string referrer =
      EvalJs(web_contents, "document.referrer").ExtractString();
  EXPECT_TRUE(referrer.empty());
}

// Test class to verify speculation hints for non-private same origin no state
// prefetches.
class SpeculationNoStatePrefetchBrowserTest
    : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    NoStatePrefetchBrowserTest::SetUp();
  }

  void InsertSpeculation(const GURL& prefetch_url,
                         FinalStatus expected_final_status,
                         bool should_navigate_away = false) {
    std::string speculation_script = R"(
      var script = document.createElement('script');
      script.type = 'speculationrules';
      script.text = `{)";
    speculation_script.append(R"("prefetch_with_subresources": [{)");
    speculation_script.append(R"("source": "list",
          "urls": [)");

    speculation_script.append("\"").append(prefetch_url.spec()).append("\"");

    speculation_script.append(R"(]
        }]
      }`;
      document.head.appendChild(script);)");
    std::unique_ptr<TestPrerender> test_prerender =
        no_state_prefetch_contents_factory()->ExpectNoStatePrefetchContents(
            expected_final_status);
    EXPECT_TRUE(ExecJs(GetActiveWebContents(), speculation_script));
    if (should_navigate_away) {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          current_browser(), src_server()->GetURL("/defaultresponse?page")));
    }
    test_prerender->WaitForStop();
  }
};

IN_PROC_BROWSER_TEST_F(SpeculationNoStatePrefetchBrowserTest,
                       SpeculationPrefetch) {
  UseHttpsSrcServer();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL("/defaultresponse?landing")));
  InsertSpeculation(src_server()->GetURL(kPrefetchPage),
                    FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

IN_PROC_BROWSER_TEST_F(SpeculationNoStatePrefetchBrowserTest,
                       SpeculationDisallowsCrossOriginRedirect) {
  UseHttpsSrcServer();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL("/defaultresponse?landing")));
  InsertSpeculation(
      src_server()->GetURL("/server-redirect-307?" +
                           src_server()->GetURL(kPrefetchPage).spec()),
      FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

IN_PROC_BROWSER_TEST_F(SpeculationNoStatePrefetchBrowserTest,
                       SpeculationAllowsSameOriginRedirectBlocked) {
  UseHttpsSrcServer();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL("/defaultresponse?landing")));
  InsertSpeculation(src_server()->GetURL(
                        "/server-redirect-307?" +
                        embedded_test_server()->GetURL(kPrefetchPage).spec()),
                    FINAL_STATUS_UNSUPPORTED_SCHEME);
  EXPECT_EQ(0u, GetRequestCount(embedded_test_server()->GetURL(kPrefetchPage)));
  EXPECT_EQ(0u,
            GetRequestCount(embedded_test_server()->GetURL(kPrefetchScript)));
}

IN_PROC_BROWSER_TEST_F(SpeculationNoStatePrefetchBrowserTest,
                       HungSpeculationTimedOutByNavigation) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  UseHttpsSrcServer();
  GetNoStatePrefetchManager()->mutable_config().abandon_time_to_live =
      base::Milliseconds(500);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL("/defaultresponse?landing")));
  InsertSpeculation(src_server()->GetURL("/hung"), FINAL_STATUS_TIMED_OUT,
                    /*should_navigate_away=*/true);
}

class NoStatePrefetchMPArchBrowserTest : public NoStatePrefetchBrowserTest {
 public:
  NoStatePrefetchMPArchBrowserTest() = default;
  ~NoStatePrefetchMPArchBrowserTest() override = default;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

class NoStatePrefetchPrerenderBrowserTest
    : public NoStatePrefetchMPArchBrowserTest {
 public:
  NoStatePrefetchPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &NoStatePrefetchPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~NoStatePrefetchPrerenderBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    NoStatePrefetchMPArchBrowserTest::SetUp();
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(NoStatePrefetchPrerenderBrowserTest,
                       ShouldNotRecordNavigation) {
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  bool recorded = GetNoStatePrefetchManager()->HasRecentlyBeenNavigatedTo(
      ORIGIN_NONE, initial_url);
  EXPECT_TRUE(recorded);

  const GURL prerender_url = embedded_test_server()->GetURL(kPrefetchPage);

  // Loads a page in the prerender.
  const content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  // NoStatePrefetchManager should not record the navigation in prerendering.
  recorded = GetNoStatePrefetchManager()->HasRecentlyBeenNavigatedTo(
      ORIGIN_NONE, prerender_url);
  EXPECT_FALSE(recorded);

  // Activate the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  recorded = GetNoStatePrefetchManager()->HasRecentlyBeenNavigatedTo(
      ORIGIN_NONE, prerender_url);
  EXPECT_TRUE(recorded);
}

class NoStatePrefetchFencedFrameBrowserTest
    : public NoStatePrefetchMPArchBrowserTest {
 public:
  NoStatePrefetchFencedFrameBrowserTest() = default;
  ~NoStatePrefetchFencedFrameBrowserTest() override = default;
  NoStatePrefetchFencedFrameBrowserTest(
      const NoStatePrefetchFencedFrameBrowserTest&) = delete;

  NoStatePrefetchFencedFrameBrowserTest& operator=(
      const NoStatePrefetchFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(NoStatePrefetchFencedFrameBrowserTest,
                       ShouldNotRecordNavigation) {
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  bool recorded = GetNoStatePrefetchManager()->HasRecentlyBeenNavigatedTo(
      ORIGIN_NONE, initial_url);
  EXPECT_TRUE(recorded);

  // Create a FencedFrame and navigate the given URL.
  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(browser()
                                                       ->tab_strip_model()
                                                       ->GetActiveWebContents()
                                                       ->GetPrimaryMainFrame(),
                                                   fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);
  // NoStatePrefetchManager should not record the navigation on fenced frame
  // navigation.
  recorded = GetNoStatePrefetchManager()->HasRecentlyBeenNavigatedTo(
      ORIGIN_NONE, fenced_frame_url);
  EXPECT_FALSE(recorded);
}

}  // namespace prerender
