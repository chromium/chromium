// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history/history_deletion_bridge.h"

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(HistoryDeletionBridge, TestSanitizeDeletionInfo) {
  history::DeletionInfo info = history::DeletionInfo::ForUrls(
      {history::URLResult(GURL("https://google.com/"), base::Time()),
       history::URLResult(GURL("https://google.com/foo"), base::Time()),
       history::URLResult(GURL("htt\\invalido\\gle.com"), base::Time()),
       history::URLResult(GURL(""), base::Time())},
      {});

  std::vector<GURL> expected = {GURL("https://google.com/"),
                                GURL("https://google.com/foo")};
  std::vector<history::URLRow> actual =
      HistoryDeletionBridge::SanitizeDeletionInfo(info).deleted_rows();
  EXPECT_EQ(expected.size(), actual.size());

  for (auto row : actual)
    EXPECT_TRUE(base::Contains(expected, row.url()));
}

TEST(HistoryDeletionBridge, TestAllHistoryDeletion) {
  history::DeletionInfo info = history::DeletionInfo::ForAllHistory();
  history::DeletionInfo sanitized_info =
      HistoryDeletionBridge::SanitizeDeletionInfo(info);
  EXPECT_TRUE(sanitized_info.IsAllHistory());
}

TEST(HistoryDeletionBridge, TestTimeRangeDeletion) {
  base::Time now = base::Time::Now();
  history::DeletionTimeRange time_range(now - base::Days(2), now);
  history::DeletionInfo info(time_range,
                             /*is_from_expiration=*/false, /*deleted_rows=*/{},
                             /*favicon_urls=*/{},
                             /*restrict_urls=*/std::nullopt);
  history::DeletionInfo sanitized_info =
      HistoryDeletionBridge::SanitizeDeletionInfo(info);
  EXPECT_EQ(time_range.begin(), sanitized_info.time_range().begin());
  EXPECT_EQ(time_range.end(), sanitized_info.time_range().end());
}
