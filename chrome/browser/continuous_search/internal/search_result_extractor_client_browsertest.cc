// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/continuous_search/browser/search_result_extractor_client.h"
#include "components/continuous_search/browser/search_result_extractor_client_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace continuous_search {

class SearchResultExtractorClientBrowserTest : public PlatformBrowserTest {
 public:
  SearchResultExtractorClientBrowserTest() {
#if BUILDFLAG(IS_ANDROID)
    feature_list_.InitWithFeatures({features::kContinuousSearch}, {});
#else
    feature_list_.InitWithFeatures(
        {base::Feature{"Journeys",
                       base::FeatureState::FEATURE_DISABLED_BY_DEFAULT}},
        {});
#endif
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

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchResultExtractorClientBrowserTest, ExtractData) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/continuous_search/results.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  SearchResultExtractorClient client(/*test_mode=*/true);
  base::RunLoop loop;
  client.RequestData(
      web_contents(), {mojom::ResultType::kSearchResults},
      base::BindOnce(
          [](base::OnceClosure quit, SearchResultExtractorClientStatus status,
             mojom::CategoryResultsPtr results) {
            EXPECT_EQ(status, SearchResultExtractorClientStatus::kSuccess);
            EXPECT_EQ(results->category_type, mojom::Category::kOrganic);
            EXPECT_EQ(results->groups.size(), 1U);
            EXPECT_EQ(results->groups[0]->type,
                      mojom::ResultType::kSearchResults);
            EXPECT_EQ(results->groups[0]->results.size(), 1U);
            EXPECT_EQ(results->groups[0]->results[0]->title, u"Bar");
            EXPECT_EQ(results->groups[0]->results[0]->link,
                      GURL("https://www.foo.com/"));
            std::move(quit).Run();
          },
          loop.QuitClosure()));
  loop.Run();
}

}  // namespace continuous_search
