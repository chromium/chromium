// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_cache.h"

#include <set>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"

namespace page_content_annotations {

namespace {

class TestPageContentCacheObserver : public PageContentCache::Observer {
 public:
  explicit TestPageContentCacheObserver(PageContentCache* cache) {
    observation_.Observe(cache);
  }

  void OnCachePopulated(int64_t tab_id) override {
    populated_tabs_.insert(tab_id);
    if (populated_run_loop_) {
      populated_run_loop_->Quit();
    }
  }

  void OnCacheRemoved(int64_t tab_id) override {
    removed_tabs_.insert(tab_id);
    if (removed_run_loop_) {
      removed_run_loop_->Quit();
    }
  }

  void WaitForPopulated(int64_t tab_id) {
    while (populated_tabs_.find(tab_id) == populated_tabs_.end()) {
      populated_run_loop_ = std::make_unique<base::RunLoop>();
      populated_run_loop_->Run();
      populated_run_loop_.reset();
    }
  }

  void WaitForRemoved(int64_t tab_id) {
    while (removed_tabs_.find(tab_id) == removed_tabs_.end()) {
      removed_run_loop_ = std::make_unique<base::RunLoop>();
      removed_run_loop_->Run();
      removed_run_loop_.reset();
    }
  }

 private:
  base::ScopedObservation<PageContentCache, PageContentCache::Observer>
      observation_{this};

  std::set<int64_t> populated_tabs_;
  std::unique_ptr<base::RunLoop> populated_run_loop_;
  std::set<int64_t> removed_tabs_;
  std::unique_ptr<base::RunLoop> removed_run_loop_;
};

}  // namespace

class PageContentCacheBrowserTest : public AndroidBrowserTest {
 public:
  PageContentCacheBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageContentCache, {}},
         {features::kAnnotatedPageContentExtraction,
          {{"capture_delay", "0s"}, {"triggering_mode", "on_load"}}}},
        {});
  }

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // FCP is flaky to wait for in test, remove this once the test is not flaky.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AndroidBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        page_content_annotations::switches::
            kPageContentAnnotationsSkipFCPWaitForTesting);
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* profile() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

  content::WebContents* AddTab(const GURL& url) {
    TabModel* tab_model =
        TabModelList::GetTabModelForWebContents(web_contents());
    CHECK(tab_model);

    TabAndroid* parent_tab = TabAndroid::FromWebContents(web_contents());

    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile()));
    content::WebContents* new_web_contents = contents.release();

    content::NavigationController::LoadURLParams params(url);
    params.transition_type =
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED);
    params.has_user_gesture = true;
    new_web_contents->GetController().LoadURLWithParams(params);
    content::WaitForLoadStop(new_web_contents);

    tab_model->CreateTab(parent_tab, new_web_contents, /*select=*/true);
    return new_web_contents;
  }

  void CloseTab(int32_t tab_id) {
    TabModel* tab_model =
        TabModelList::GetTabModelForWebContents(web_contents());
    ASSERT_TRUE(tab_model);
    int index_to_close = -1;
    for (int i = 0; i < tab_model->GetTabCount(); ++i) {
      TabAndroid* tab = tab_model->GetTabAt(i);
      if (tab && tab->GetAndroidId() == tab_id) {
        index_to_close = i;
        break;
      }
    }
    ASSERT_NE(index_to_close, -1);
    tab_model->CloseTabAt(index_to_close);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/453977731): Flaky test.
IN_PROC_BROWSER_TEST_F(PageContentCacheBrowserTest,
                       DISABLED_CacheBehaviorOnTabSwitchAndClose) {
  auto* extraction_service =
      PageContentExtractionServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(extraction_service);
  auto* cache = extraction_service->GetPageContentCache();
  ASSERT_TRUE(cache);
  TestPageContentCacheObserver cache_observer(cache);

  GURL url1(embedded_test_server()->GetURL("a.test",
                                           "/optimization_guide/hello.html"));

  // Open tab1 and wait for page load
  content::WebContents* wc1 = web_contents();
  ASSERT_TRUE(content::NavigateToURL(wc1, url1));
  TabAndroid* tab1 = TabAndroid::FromWebContents(wc1);
  int32_t tab_id1 = tab1->GetAndroidId();

  // Push tab1 to background, should add contents to cache.
  AddTab(url1);

  cache_observer.WaitForPopulated(tab_id1);
  base::test::TestFuture<std::optional<optimization_guide::proto::PageContext>>
      future1;
  cache->GetPageContentForTab(tab_id1, future1.GetCallback());
  auto result1 = future1.Get();
  EXPECT_TRUE(result1.has_value());

  // Close tab: tab_id1, it should delete the cached contents.
  CloseTab(tab_id1);

  cache_observer.WaitForRemoved(tab_id1);
  base::test::TestFuture<std::optional<optimization_guide::proto::PageContext>>
      future_after_close;
  cache->GetPageContentForTab(tab_id1, future_after_close.GetCallback());
  auto result_after_close = future_after_close.Get();
  EXPECT_FALSE(result_after_close.has_value());
}

}  // namespace page_content_annotations
