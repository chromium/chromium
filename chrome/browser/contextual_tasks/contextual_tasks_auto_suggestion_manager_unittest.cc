// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_auto_suggestion_manager.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

class ContextualTasksAutoSuggestionManagerTest : public testing::Test {
 public:
  ContextualTasksAutoSuggestionManagerTest() = default;

 protected:
  std::unique_ptr<SuggestedTabInfo> CreateTabInfo(const std::string& url_str) {
    auto info = std::make_unique<SuggestedTabInfo>();
    info->url = GURL(url_str);
    info->title = u"Test Title";
    return info;
  }

  ContextualTasksAutoSuggestionManager manager_;
};

TEST_F(ContextualTasksAutoSuggestionManagerTest, SetCurrentSuggestion) {
  auto info = CreateTabInfo("https://example.com");
  manager_.SetCurrentSuggestion(std::move(info));

  const SuggestedTabInfo* current = manager_.GetCurrentSuggestion();
  ASSERT_NE(current, nullptr);
  EXPECT_EQ(current->url, GURL("https://example.com"));

  // Set to null
  manager_.SetCurrentSuggestion(nullptr);
  EXPECT_EQ(manager_.GetCurrentSuggestion(), nullptr);
}

TEST_F(ContextualTasksAutoSuggestionManagerTest, OnAutoSuggestionDismissed) {
  GURL url("https://example.com");
  manager_.SetCurrentSuggestion(CreateTabInfo(url.spec()));

  // Dismiss
  manager_.OnAutoSuggestionDismissed();
  EXPECT_EQ(manager_.GetCurrentSuggestion(), nullptr);

  // Try to set the same URL again - should be ignored due to blocklist.
  manager_.SetCurrentSuggestion(CreateTabInfo(url.spec()));
  EXPECT_EQ(manager_.GetCurrentSuggestion(), nullptr);
}

TEST_F(ContextualTasksAutoSuggestionManagerTest,
       OnTabContextAddedRemovesFromBlocklist) {
  GURL url("https://example.com");
  manager_.SetCurrentSuggestion(CreateTabInfo(url.spec()));
  manager_.OnAutoSuggestionDismissed();
  ASSERT_EQ(manager_.GetCurrentSuggestion(), nullptr);

  // Mark as added (manual add)
  manager_.OnTabContextAdded(url, /*is_active_tab=*/false);

  // Should be allowed again
  manager_.SetCurrentSuggestion(CreateTabInfo(url.spec()));
  const SuggestedTabInfo* current = manager_.GetCurrentSuggestion();
  ASSERT_NE(current, nullptr);
  EXPECT_EQ(current->url, url);
}

TEST_F(ContextualTasksAutoSuggestionManagerTest,
       OnTabContextRemovedClearsMatchingSuggestion) {
  GURL url("https://example.com");
  manager_.SetCurrentSuggestion(CreateTabInfo(url.spec()));
  ASSERT_NE(manager_.GetCurrentSuggestion(), nullptr);

  // Remove the same URL.
  manager_.OnTabContextRemoved(url);
  EXPECT_EQ(manager_.GetCurrentSuggestion(), nullptr);

  // Should be blocklisted now.
  manager_.SetCurrentSuggestion(CreateTabInfo(url.spec()));
  EXPECT_EQ(manager_.GetCurrentSuggestion(), nullptr);
}

TEST_F(ContextualTasksAutoSuggestionManagerTest, ResetClearsEverything) {
  GURL url("https://example.com");
  manager_.SetCurrentSuggestion(CreateTabInfo(url.spec()));
  manager_.OnAutoSuggestionDismissed();
  ASSERT_EQ(manager_.GetCurrentSuggestion(), nullptr);

  manager_.Reset();

  // Blocklist should be clear
  manager_.SetCurrentSuggestion(CreateTabInfo(url.spec()));
  EXPECT_NE(manager_.GetCurrentSuggestion(), nullptr);
}

TEST_F(ContextualTasksAutoSuggestionManagerTest, RecordMetricOnActiveTabAdded) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  GURL url("https://example.com");
  const std::string metric_name =
      "ContextualTasks.Composebox.UserAction."
      "AddedActiveTabAfterDeletingAutoSuggestion";

  // 1. Add active tab without blocklist.
  manager_.OnTabContextAdded(url, /*is_active_tab=*/true);
  histogram_tester.ExpectTotalCount(metric_name, 0);

  // 2. Blocklist a URL.
  manager_.OnTabContextRemoved(url);

  // 3. Add active tab with non-empty blocklist.
  manager_.OnTabContextAdded(url, /*is_active_tab=*/true);
  histogram_tester.ExpectTotalCount(metric_name, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(metric_name), 1);
}

TEST_F(ContextualTasksAutoSuggestionManagerTest, SwitchingSuggestions) {
  manager_.SetCurrentSuggestion(CreateTabInfo("https://a.com"));
  EXPECT_EQ(manager_.GetCurrentSuggestion()->url, GURL("https://a.com"));

  manager_.SetCurrentSuggestion(CreateTabInfo("https://b.com"));
  EXPECT_EQ(manager_.GetCurrentSuggestion()->url, GURL("https://b.com"));
}

}  // namespace contextual_tasks
