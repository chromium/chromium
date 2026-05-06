// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)

#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/accessibility_annotator/accessibility_query_service_delegate_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_content_annotations/content/mock_page_content_services.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace accessibility_annotator {

using ::page_content_annotations::CreateExtractionResult;
using ::page_content_annotations::CreatePassageEmbeddings;
using ::page_content_annotations::MockPageContentExtractionService;
using ::page_content_annotations::MockPageEmbeddingsService;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

class AccessibilityQueryServiceDelegateImplBrowserTest
    : public InProcessBrowserTest {
 public:
  AccessibilityQueryServiceDelegateImplBrowserTest() = default;
  ~AccessibilityQueryServiceDelegateImplBrowserTest() override = default;
};

// RetrieveLiveTabContext returns an empty response when dependencies missing.
IN_PROC_BROWSER_TEST_F(AccessibilityQueryServiceDelegateImplBrowserTest,
                       RetrieveLiveTabContext_MissingDependencies) {
  AccessibilityQueryServiceDelegateImpl delegate(browser()->profile());

  LiveTabContextQuery query;
  query.query = u"test query";

  base::test::TestFuture<LiveTabContextResponse> future;
  delegate.RetrieveLiveTabContext(query, future.GetCallback());

  LiveTabContextResponse response = future.Take();
  EXPECT_TRUE(response.results.empty());
}

// RetrieveLiveTabContext returns valid response when dependencies available.
IN_PROC_BROWSER_TEST_F(AccessibilityQueryServiceDelegateImplBrowserTest,
                       RetrieveLiveTabContext_Success) {
  // Setup mock services.
  Profile* profile = browser()->profile();
  NiceMock<MockPageContentExtractionService> mock_extraction_service;
  NiceMock<MockPageEmbeddingsService> mock_embeddings_service(
      &mock_extraction_service);
  passage_embeddings::TestEmbedder test_embedder;

  EXPECT_CALL(mock_extraction_service,
              GetExtractedPageContentAndEligibilityForPage)
      .WillOnce(Return(CreateExtractionResult("passage 1", true)))
      .WillOnce(Return(CreateExtractionResult("passage 2", true)))
      .WillOnce(Return(CreateExtractionResult("passage 3", true)));

  EXPECT_CALL(mock_embeddings_service, GetEmbeddings)
      .WillOnce(Return(CreatePassageEmbeddings("passage 1")))
      .WillOnce(Return(CreatePassageEmbeddings("passage 2")))
      .WillOnce(Return(CreatePassageEmbeddings("passage 3")));

  AccessibilityQueryServiceDelegateImpl delegate(
      profile, &mock_extraction_service, &mock_embeddings_service,
      &test_embedder);

  // Create 3 tabs.
  // The browser starts with 1 tab by default. We add 2 more.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  for (int i = 0; i < 2; ++i) {
    content::WebContents* web_contents = chrome::AddSelectedTabWithURL(
        browser(), GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
    content::WaitForLoadStop(web_contents);
  }
  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);

  LiveTabContextQuery query;
  query.query = u"test query";

  base::test::TestFuture<LiveTabContextResponse> future;
  delegate.RetrieveLiveTabContext(query, future.GetCallback());

  LiveTabContextResponse response = future.Take();

  // We expect 3 results (1 from each tab).
  EXPECT_THAT(response.results,
              UnorderedElementsAre(u"passage 1", u"passage 2", u"passage 3"));
}

}  // namespace accessibility_annotator

#endif  // !BUILDFLAG(IS_ANDROID)
