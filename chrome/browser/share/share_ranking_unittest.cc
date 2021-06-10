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

TEST_F(ShareRankingTest, DISABLED_EmptyOldRankingReflectsHistory) {
  std::map<std::string, int> history = {
      {"bar", 2},
      {"foo", 3},
      {"baz", 1},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::ComputeRanking(history, history, {}, {"foo", "bar", "baz"}, 3,
                               &displayed, &persisted);

  ShareRanking::Ranking expected{"foo", "bar", "baz"};
  EXPECT_EQ(displayed, expected);
  EXPECT_EQ(persisted, expected);
}

TEST_F(ShareRankingTest, DISABLED_CountsMatchOldRanking) {
  std::map<std::string, int> history = {
      {"bar", 2},
      {"foo", 3},
      {"baz", 1},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "baz"};

  ShareRanking::ComputeRanking(history, history, current, {"foo", "bar", "baz"},
                               3, &displayed, &persisted);
  EXPECT_EQ(displayed, current);
  EXPECT_EQ(persisted, current);
}

TEST_F(ShareRankingTest, DISABLED_UnavailableAppDoesNotShow) {
  std::map<std::string, int> history = {
      {"foo", 5}, {"bar", 4}, {"baz", 3}, {"quxx", 2}, {"blit", 1},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "baz", "quxx", "blit"};

  ShareRanking::ComputeRanking(history, history, current,
                               {"foo", "quxx", "baz", "blit"}, 4, &displayed,
                               &persisted);
  ShareRanking::Ranking expected_displayed{
      "foo",
      "baz",
      "quxx",
      "bliz",
  };
  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, current);
}

}  // namespace sharing
