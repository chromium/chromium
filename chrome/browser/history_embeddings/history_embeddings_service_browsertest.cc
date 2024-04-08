// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "content/public/test/browser_test.h"

namespace history_embeddings {

class HistoryEmbeddingsBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    // The feature must be enabled first or else the service isn't initialized
    // properly.
    feature_list_.InitAndEnableFeature(kHistoryEmbeddings);

    InProcessBrowserTest::SetUp();
  }

 protected:
  HistoryEmbeddingsService* service() {
    return HistoryEmbeddingsServiceFactory::GetForProfile(browser()->profile());
  }

  base::RepeatingCallback<void(UrlPassages)>& callback_for_tests() {
    return service()->callback_for_tests_;
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest, ServiceFactoryWorks) {
  auto* service =
      HistoryEmbeddingsServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(service);
}

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest, BrowserRetrievesPassages) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/inner_text/test1.html")));

  base::test::TestFuture<UrlPassages> future;
  callback_for_tests() = future.GetRepeatingCallback();
  service()->RetrievePassages({}, *web_contents->GetPrimaryMainFrame());

  UrlPassages url_passages = future.Take();

  // Note: Currently the passage extraction algorithm does not recurse
  // into iframes. If that changes then the passage structure and content
  // here will need to change accordingly.
  ASSERT_EQ(url_passages.passages.passages_size(), 1);
  ASSERT_EQ(url_passages.passages.passages(0), "A B C D");
}

IN_PROC_BROWSER_TEST_F(HistoryEmbeddingsBrowserTest,
                       SearchFindsResultWithSourcePassage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::TestFuture<UrlPassages> store_future;
  callback_for_tests() = store_future.GetRepeatingCallback();

  const GURL url = embedded_test_server()->GetURL("/inner_text/test1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(store_future.Wait());

  base::HistogramTester histogram_tester;

  // Search for the passage.
  base::test::TestFuture<SearchResult> search_future;
  service()->Search("A B C D e f g", 1, search_future.GetCallback());
  SearchResult result = search_future.Take();
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].scored_url.passage, "A B C D");
  EXPECT_EQ(result[0].row.url(), url);

  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.QueryEmbeddingSucceeded", true, 1);
}

}  // namespace history_embeddings
