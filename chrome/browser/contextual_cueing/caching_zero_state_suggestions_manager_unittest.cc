// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/caching_zero_state_suggestions_manager.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/mock_contextual_cueing_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_cueing {
namespace {
void NavigateAndCommitContents(content::WebContents* contents,
                               const GURL& url) {
  content::WebContentsTester* tester =
      content::WebContentsTester::For(contents);
  tester->NavigateAndCommit(url);
}

class TestContextualCueingService
    : public contextual_cueing::MockContextualCueingService {
 public:
  void GetContextualGlicZeroStateSuggestionsForFocusedTab(
      content::WebContents* web_contents,
      bool is_fre,
      std::optional<std::vector<std::string>> supported_tools,
      GlicSuggestionsCallback callback) override {
    if (last_callback_) {
      pending_callbacks_.push_back(std::move(last_callback_));
    }
    last_callback_ = std::move(callback);
  }

  bool GetContextualGlicZeroStateSuggestionsForPinnedTabs(
      std::vector<content::WebContents*> pinned_web_contents,
      bool is_fre,
      std::optional<std::vector<std::string>> supported_tools,
      const content::WebContents* focused_tab,
      GlicSuggestionsCallback callback) override {
    if (last_callback_) {
      pending_callbacks_.push_back(std::move(last_callback_));
    }
    last_callback_ = std::move(callback);
    return will_generate_pinned_tab_suggestions_;
  }

  bool will_generate_pinned_tab_suggestions_ = false;
  contextual_cueing::GlicSuggestionsCallback last_callback_;
  std::vector<contextual_cueing::GlicSuggestionsCallback> pending_callbacks_;
};

class CachingContextualCueingServiceTest : public testing::Test {
 public:
  std::unique_ptr<content::WebContents> CreateWebContentsAt(GURL url) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    NavigateAndCommitContents(contents.get(), GURL("https://www.google.com"));
    return contents;
  }

  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
};

TEST_F(CachingContextualCueingServiceTest, FocusedTabRequestIsPassedThrough) {
  TestContextualCueingService contextual_cueing_service;
  auto cache =
      CreateCachingZeroStateSuggestionsManager(&contextual_cueing_service);
  auto focus = CreateWebContentsAt(GURL("https://www.google.com"));
  base::test::TestFuture<std::vector<std::string>> result;
  cache->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      focus.get(), true, std::nullopt, result.GetCallback());

  std::move(contextual_cueing_service.last_callback_).Run({"S1"});
  EXPECT_THAT(result.Get(), testing::ElementsAre("S1"));
}

TEST_F(CachingContextualCueingServiceTest,
       PinnedTabRequestRequestIsPassedThrough) {
  TestContextualCueingService contextual_cueing_service;
  auto cache =
      CreateCachingZeroStateSuggestionsManager(&contextual_cueing_service);
  contextual_cueing_service.will_generate_pinned_tab_suggestions_ = true;
  auto focus = CreateWebContentsAt(GURL("https://www.google.com"));
  base::test::TestFuture<std::vector<std::string>> result;
  EXPECT_TRUE(cache->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
      {focus.get()}, true, std::nullopt, focus.get(), result.GetCallback()));

  std::move(contextual_cueing_service.last_callback_).Run({"S1"});
  EXPECT_THAT(result.Get(), testing::ElementsAre("S1"));
}

TEST_F(CachingContextualCueingServiceTest,
       PinnedTabRequestRequestIsPassedThroughContextualNotGenerated) {
  TestContextualCueingService contextual_cueing_service;
  auto cache =
      CreateCachingZeroStateSuggestionsManager(&contextual_cueing_service);
  contextual_cueing_service.will_generate_pinned_tab_suggestions_ = false;
  auto focus = CreateWebContentsAt(GURL("https://www.google.com"));
  base::test::TestFuture<std::vector<std::string>> result;
  EXPECT_FALSE(cache->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
      {focus.get()}, true, std::nullopt, focus.get(), result.GetCallback()));

  std::move(contextual_cueing_service.last_callback_).Run({});
  EXPECT_THAT(result.Get(), testing::ElementsAre());
}

TEST_F(CachingContextualCueingServiceTest,
       PinnedTabRequestRequestForSameSetOfTabs) {
  TestContextualCueingService contextual_cueing_service;
  auto cache =
      CreateCachingZeroStateSuggestionsManager(&contextual_cueing_service);
  contextual_cueing_service.will_generate_pinned_tab_suggestions_ = true;
  auto focus = CreateWebContentsAt(GURL("https://www.google.com"));
  base::test::TestFuture<std::vector<std::string>> result;
  EXPECT_TRUE(cache->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
      {focus.get()}, true, std::nullopt, focus.get(), result.GetCallback()));
  base::test::TestFuture<std::vector<std::string>> result2;
  EXPECT_FALSE(cache->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
      {focus.get()}, true, std::nullopt, focus.get(), result2.GetCallback()));

  std::move(contextual_cueing_service.last_callback_).Run({"S1"});
  EXPECT_THAT(result.Get(), testing::ElementsAre("S1"));
  EXPECT_THAT(result2.Get(), testing::ElementsAre("S1"));
}

TEST_F(CachingContextualCueingServiceTest, CachedResultIsReturned) {
  TestContextualCueingService contextual_cueing_service;
  auto cache =
      CreateCachingZeroStateSuggestionsManager(&contextual_cueing_service);
  auto focus = CreateWebContentsAt(GURL("https://www.google.com"));
  base::test::TestFuture<std::vector<std::string>> result1;
  cache->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      focus.get(), true, std::nullopt, result1.GetCallback());

  std::move(contextual_cueing_service.last_callback_).Run({"S1"});
  EXPECT_THAT(result1.Get(), testing::ElementsAre("S1"));

  base::test::TestFuture<std::vector<std::string>> result2;
  cache->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      focus.get(), true, std::nullopt, result2.GetCallback());
  EXPECT_THAT(result2.Get(), testing::ElementsAre("S1"));
}

TEST_F(CachingContextualCueingServiceTest,
       SecondRequestWithoutResultIsCoalesced) {
  TestContextualCueingService contextual_cueing_service;
  auto cache =
      CreateCachingZeroStateSuggestionsManager(&contextual_cueing_service);
  auto focus = CreateWebContentsAt(GURL("https://www.google.com"));
  base::test::TestFuture<std::vector<std::string>> result1;
  cache->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      focus.get(), true, std::nullopt, result1.GetCallback());
  base::test::TestFuture<std::vector<std::string>> result2;
  cache->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      focus.get(), true, std::nullopt, result2.GetCallback());

  std::move(contextual_cueing_service.last_callback_).Run({"S1"});
  EXPECT_THAT(result1.Get(), testing::ElementsAre("S1"));
  EXPECT_THAT(result2.Get(), testing::ElementsAre("S1"));
}

TEST_F(CachingContextualCueingServiceTest, CacheMissTriggersNewRequest) {
  TestContextualCueingService contextual_cueing_service;
  auto cache =
      CreateCachingZeroStateSuggestionsManager(&contextual_cueing_service);
  auto focus1 = CreateWebContentsAt(GURL("https://www.google.com/tab1"));
  auto focus2 = CreateWebContentsAt(GURL("https://www.google.com/tab2"));

  // First request, populates cache.
  base::test::TestFuture<std::vector<std::string>> result1;
  cache->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      focus1.get(), true, std::nullopt, result1.GetCallback());
  std::move(contextual_cueing_service.last_callback_).Run({"S1"});
  EXPECT_THAT(result1.Get(), testing::ElementsAre("S1"));

  // Second request with different WebContents, should be a cache miss.
  base::test::TestFuture<std::vector<std::string>> result2;
  cache->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      focus2.get(), true, std::nullopt, result2.GetCallback());

  // Verify that a new request was made to the underlying service.
  // This is implicitly checked by the fact that last_callback_ is updated.
  std::move(contextual_cueing_service.last_callback_).Run({"S2"});
  EXPECT_THAT(result2.Get(), testing::ElementsAre("S2"));
}

// Process enough requests to force an eviction. Each request gets a response
// before eviction.
TEST_F(CachingContextualCueingServiceTest, EvictCompleteCacheEntries) {
  TestContextualCueingService contextual_cueing_service;
  auto cache =
      CreateCachingZeroStateSuggestionsManager(&contextual_cueing_service);
  for (int i = 0; i < 12; ++i) {
    auto focus = CreateWebContentsAt(
        GURL("https://www.google.com/tab" + base::NumberToString(i)));
    base::test::TestFuture<std::vector<std::string>> result;
    std::string suggestion = "S" + base::NumberToString(i);
    cache->GetContextualGlicZeroStateSuggestionsForFocusedTab(
        focus.get(), true, std::nullopt, result.GetCallback());
    std::move(contextual_cueing_service.last_callback_).Run({suggestion});
    EXPECT_THAT(result.Get(), testing::ElementsAre(suggestion));
  }
}

// Process enough requests to force an eviction, but only complete requests at
// the end. Evicted entries will invoke their callbacks before getting a
// response.
TEST_F(CachingContextualCueingServiceTest, EvictIncompleteCacheEntries) {
  TestContextualCueingService contextual_cueing_service;
  std::vector<std::string> suggestions_received;
  auto callback =
      base::BindLambdaForTesting([&](std::vector<std::string> suggestions) {
        suggestions_received.push_back(suggestions.empty() ? "NONE"
                                                           : suggestions[0]);
      });

  auto cache =
      CreateCachingZeroStateSuggestionsManager(&contextual_cueing_service);
  for (int i = 0; i < 12; ++i) {
    auto focus = CreateWebContentsAt(
        GURL("https://www.google.com/tab" + base::NumberToString(i)));
    base::test::TestFuture<std::vector<std::string>> result;
    cache->GetContextualGlicZeroStateSuggestionsForFocusedTab(
        focus.get(), true, std::nullopt, callback);
  }

  ASSERT_THAT(suggestions_received, testing::ElementsAre("NONE", "NONE"));

  // Resolve all requests.
  int i = 0;
  for (auto& cb : contextual_cueing_service.pending_callbacks_) {
    std::string suggestion = "S" + base::NumberToString(i++);
    std::move(cb).Run({suggestion});
  }
  std::string suggestion = "S" + base::NumberToString(i++);
  std::move(contextual_cueing_service.last_callback_).Run({suggestion});

  ASSERT_THAT(suggestions_received,
              testing::ElementsAre("NONE", "NONE", "S2", "S3", "S4", "S5", "S6",
                                   "S7", "S8", "S9", "S10", "S11"));
}

}  // namespace
}  // namespace contextual_cueing
