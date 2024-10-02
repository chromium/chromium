// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/chrome_prefetch_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prefetch_test_util.h"
#include "content/public/test/preloading_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

class PrefetchBrowserTest : public PlatformBrowserTest {
 public:
  PrefetchBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kPrefetchBrowserInitiatedTriggers};

#if BUILDFLAG(IS_ANDROID)
    enabled_features.push_back(chrome::android::kCCTNavigationalPrefetch);
#endif  // BUILDFLAG(IS_ANDROID)

    feature_list_.InitWithFeatures(enabled_features, {});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ssl_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(ssl_server_.Start());

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    attempt_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            PredictorToExpectInUkm());
    test_timer_ = std::make_unique<base::ScopedMockElapsedTimersForTest>();
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    ASSERT_TRUE(ssl_server_.ShutdownAndWaitUntilComplete());
  }

  GURL GetURL(const std::string& path) {
    return ssl_server_.GetURL("a.test", path);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  bool NavigateToURL(const GURL& url) {
    return content::NavigateToURL(GetActiveWebContents(), url);
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const content::test::PreloadingAttemptUkmEntryBuilder&
  attempt_entry_builder() {
    return *attempt_entry_builder_;
  }

 protected:
  // Used when creating `test_ukm_recorder_`.
  content::PreloadingPredictor PredictorToExpectInUkm() {
    return chrome_preloading_predictor::kChromeCustomTabs;
  }

 private:
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      attempt_entry_builder_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> test_timer_;
};

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTest, CCTPrefetch) {
  content::test::TestPrefetchWatcher test_prefetch_watcher;

  const GURL initial_url = GetURL("/empty.html");
  const GURL prefetch_url = GetURL("/simple.html");
  ASSERT_TRUE(NavigateToURL(initial_url));

  auto* chrome_prefetch_manager =
      ChromePrefetchManager::GetOrCreateForWebContents(GetActiveWebContents());
  chrome_prefetch_manager->StartPrefetchFromCCT(prefetch_url, false,
                                                std::nullopt);

  content::test::PrefetchContainerIdForTesting prefetch_container_id =
      test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                                               prefetch_url);

  ASSERT_TRUE(NavigateToURL(prefetch_url));

  EXPECT_TRUE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
  EXPECT_EQ(
      test_prefetch_watcher.GetPrefetchContainerIdForTestingInLastNavigation(),
      prefetch_container_id);

  auto cct_attempt_entry_builder =
      std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
          chrome_preloading_predictor::kChromeCustomTabs);

  ukm::SourceId ukm_source_id =
      GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  content::test::ExpectPreloadingAttemptUkm(
      *test_ukm_recorder(),
      {cct_attempt_entry_builder->BuildEntry(
          ukm_source_id, content::PreloadingType::kPrefetch,
          content::PreloadingEligibility::kEligible,
          content::PreloadingHoldbackStatus::kAllowed,
          content::PreloadingTriggeringOutcome::kSuccess,
          content::PreloadingFailureReason::kUnspecified,
          /*accurate=*/true,
          /*ready_time=*/
          base::ScopedMockElapsedTimersForTest::kMockElapsedTime)});
}

class CCTPrerenderBrowserTestWithHoldback : public PrefetchBrowserTest {
 public:
  CCTPrerenderBrowserTestWithHoldback() {
    feature_list_.InitAndEnableFeatureWithParameters(
        chrome::android::kCCTNavigationalPrefetch, {{"holdback", "true"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CCTPrerenderBrowserTestWithHoldback,
                       CCTPrefetchHoldback) {
  const GURL initial_url = GetURL("/empty.html");
  const GURL prefetch_url = GetURL("/simple.html");
  ASSERT_TRUE(NavigateToURL(initial_url));

  auto* chrome_prefetch_manager =
      ChromePrefetchManager::GetOrCreateForWebContents(GetActiveWebContents());
  chrome_prefetch_manager->StartPrefetchFromCCT(prefetch_url, false,
                                                std::nullopt);

  ASSERT_TRUE(NavigateToURL(prefetch_url));

  auto cct_attempt_entry_builder =
      std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
          chrome_preloading_predictor::kChromeCustomTabs);

  ukm::SourceId ukm_source_id =
      GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  content::test::ExpectPreloadingAttemptUkm(
      *test_ukm_recorder(),
      {cct_attempt_entry_builder->BuildEntry(
          ukm_source_id, content::PreloadingType::kPrefetch,
          content::PreloadingEligibility::kEligible,
          content::PreloadingHoldbackStatus::kHoldback,
          content::PreloadingTriggeringOutcome::kUnspecified,
          content::PreloadingFailureReason::kUnspecified,
          /*accurate=*/true,
          /*ready_time=*/std::nullopt)});
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
