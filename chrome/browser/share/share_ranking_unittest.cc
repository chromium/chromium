// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_ranking.h"

#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharing {

class ShareRankingTest : public testing::Test {
 public:
  using FakeDB = leveldb_proto::test::FakeDB<proto::ShareRanking>;

  ShareRankingTest() { Init(); }

  void Init(bool do_default_init = true) {
    auto backing_db = std::make_unique<FakeDB>(&backing_entries_);
    backing_db_ = backing_db.get();

    db_ = std::make_unique<ShareRanking>(
        &profile_, base::WrapUnique(backing_db.release()));

    if (do_default_init) {
      backing_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    }
  }

 private:
  content::BrowserTaskEnvironment environment_{
      // This set of tests must use a mock time source. If they don't, and a
      // test happens to run across UTC midnight, the day can change mid-test,
      // causing surprising results.
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  std::unique_ptr<ShareRanking> db_;
  leveldb_proto::test::FakeDB<proto::ShareRanking>::EntryMap backing_entries_;
  leveldb_proto::test::FakeDB<proto::ShareRanking>* backing_db_ = nullptr;
};

// The "easy case": the existing usage counts are the same as the current
// ranking, and every app in the ranking is available, so the new ranking and
// the displayed ranking should both be the same as the current ranking.
TEST_F(ShareRankingTest, CountsMatchOldRanking) {
  std::map<std::string, int> history = {
      {"bar", 2},
      {"foo", 3},
      {"baz", 1},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "baz"};

  ShareRanking::ComputeRanking(history, history, current, {"foo", "bar", "baz"},
                               3, false, &displayed, &persisted);
  ASSERT_EQ(displayed.size(), current.size());
  ASSERT_EQ(persisted.size(), current.size());
  EXPECT_EQ(displayed, current);
  EXPECT_EQ(persisted, current);
}

// If the existing ranking includes an above-the-fold app that doesn't exist on
// the system, that app should be replaced by the next available below-the-fold
// app that does.
TEST_F(ShareRankingTest, UnavailableAppDoesNotShow) {
  std::map<std::string, int> history = {
      {"foo", 5}, {"bar", 4}, {"baz", 3}, {"quxx", 2}, {"blit", 1},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "baz", "quxx", "blit"};

  ShareRanking::ComputeRanking(history, {}, current,
                               {"foo", "quxx", "baz", "blit"}, 4, false,
                               &displayed, &persisted);
  ShareRanking::Ranking expected_displayed{
      "foo",
      "blit",
      "baz",
      "quxx",
  };
  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, current);
}

TEST_F(ShareRankingTest, HighAllUsageAppReplacesLowest) {
  std::map<std::string, int> history = {
      {"foo", 5}, {"bar", 4}, {"baz", 3}, {"quxx", 2}, {"blit", 10}};

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "baz", "quxx", "blit"};
  ShareRanking::ComputeRanking(history, {}, current,
                               {"foo", "bar", "quxx", "baz", "blit"}, 4, false,
                               &displayed, &persisted);
  ShareRanking::Ranking expected_displayed{
      "foo",
      "bar",
      "baz",
      "blit",
  };
  ShareRanking::Ranking expected_persisted{"foo", "bar", "baz", "blit", "quxx"};
  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, expected_persisted);
}

TEST_F(ShareRankingTest, HighRecentUsageAppReplacesLowest) {
  std::map<std::string, int> history = {
      {"foo", 5}, {"bar", 4}, {"baz", 3}, {"quxx", 2}, {"blit", 6}};

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "baz", "quxx", "blit"};
  ShareRanking::ComputeRanking({}, history, current,
                               {"foo", "bar", "quxx", "baz", "blit"}, 4, false,
                               &displayed, &persisted);
  ShareRanking::Ranking expected_displayed{
      "foo",
      "bar",
      "baz",
      "blit",
  };
  ShareRanking::Ranking expected_persisted{"foo", "bar", "baz", "blit", "quxx"};
  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, expected_persisted);
}

TEST_F(ShareRankingTest, MoreTargetReplacesLast) {
  std::map<std::string, int> history = {
      {"bar", 2},
      {"foo", 3},
      {"baz", 1},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "baz"};

  ShareRanking::ComputeRanking(history, history, current, {"foo", "bar", "baz"},
                               3, true, &displayed, &persisted);

  std::vector<std::string> expected_displayed{"foo", "bar",
                                              ShareRanking::kMoreTarget};

  ASSERT_EQ(displayed.size(), expected_displayed.size());
  ASSERT_EQ(persisted.size(), current.size());
  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, current);
}

}  // namespace sharing
