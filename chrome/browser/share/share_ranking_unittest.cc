// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_ranking.h"

#include "base/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/share/fake_share_history.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharing {

// The static tests are true unit tests that cover the static
// ShareRanking::ComputeHistory method only; to enforce that, they don't set up
// a test fixture, which will cause calls to other ShareRanking methods to
// crash.
using ShareRankingStaticTest = testing::Test;

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

  ShareRanking* db() { return db_.get(); }
  leveldb_proto::test::FakeDB<proto::ShareRanking>* backing_db() {
    return backing_db_;
  }
  leveldb_proto::test::FakeDB<proto::ShareRanking>::EntryMap*
  backing_entries() {
    return &backing_entries_;
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
TEST(ShareRankingStaticTest, CountsMatchOldRanking) {
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
TEST(ShareRankingStaticTest, UnavailableAppDoesNotShow) {
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

TEST(ShareRankingStaticTest, HighAllUsageAppReplacesLowest) {
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

TEST(ShareRankingStaticTest, HighRecentUsageAppReplacesLowest) {
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

TEST(ShareRankingStaticTest, MoreTargetReplacesLast) {
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

// Regression test for https://crbug.com/1233232
TEST_F(ShareRankingTest, OldRankingContainsItemsWithNoRecentHistory) {
  std::map<std::string, int> history = {
      {"bar", 2},
      {"foo", 3},
      {"abc", 4},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "baz", "abc"};

  ShareRanking::ComputeRanking(history, history, current, {"foo", "bar", "baz"},
                               3, false, &displayed, &persisted);

  // Note that since "abc" isn't available on the system, it won't appear in the
  // displayed ranking, but the persisted ranking should still get updated.
  ShareRanking::Ranking expected_displayed{"foo", "bar", "baz"};
  ShareRanking::Ranking expected_persisted{"foo", "bar", "abc", "baz"};

  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, expected_persisted);
}

TEST_F(ShareRankingTest, InitialStateNoHistory) {
  FakeShareHistory history;

  history.set_history({});

  // Avoid tests depending on the actual locale (!)
  db()->set_initial_ranking_for_test(ShareRanking::Ranking{
      "aaa",
      "bbb",
      "ccc",
      "ddd",
      "eee",
      "fff",
      "ggg",
      "hhh",
      "iii",
      "jjj",
  });

  base::RunLoop loop;
  absl::optional<ShareRanking::Ranking> ranking;
  auto callback =
      base::BindLambdaForTesting([&](absl::optional<ShareRanking::Ranking> r) {
        ranking = r;
        loop.Quit();
      });

  // TODO(ellyjones): figure out a better way to do this.
  // This is currently required because internally, Rank() makes three async
  // calls - two to ShareHistory::GetFlatShareHistory(), and one to
  // ShareRanking::GetRanking(). The two calls to GetFlatShareHistory()
  // automatically post their results (since we use FakeShareHistory here, which
  // has that behavior), but the call to ShareRanking::GetRanking() turns into a
  // call to leveldb_proto::test::FakeDB::LoadEntries(), which won't run its
  // completion callback until leveldb_proto::test::FakeDB::GetCallback() is
  // invoked - that is, test code has to manually complete the get.
  //
  // However, we can't simply post a normal task to complete that call, because
  // that will happen "too early" (before LoadEntries is called). In lieu of
  // that, we post a delayed task with a long time delay. Since these tests run
  // under TimeSource::MOCK_TIME, this delayed task will actually run instantly
  // as soon as the run loop becomes idle, because the mock clock advances only
  // under those conditions, so this serves as a "run once we are blocked"
  // primitive. This doesn't actually delay the test by 10 seconds.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindLambdaForTesting([&]() { backing_db()->GetCallback(true); }),
      base::TimeDelta::FromSeconds(10));

  db()->Rank(&history, "type", {"aaa", "ccc", "eee", "ggg", "iii"}, 4, true,
             std::move(callback));
  loop.Run();

  ASSERT_TRUE(ranking);
  // The displayed ranking should only contain apps available on the system.
  EXPECT_EQ(*ranking, std::vector<std::string>({"aaa", "eee", "ccc", "$more"}));

  backing_db()->UpdateCallback(true);
  auto entries = backing_entries()->at("type");
  // The stored ranking should be identical to the default ranking we injected
  // above, since there's no history to influence it.
  EXPECT_EQ(entries.targets().at(0), "aaa");
  EXPECT_EQ(entries.targets().at(1), "bbb");
  EXPECT_EQ(entries.targets().at(2), "ccc");
}

TEST_F(ShareRankingTest, DISABLED_AllHistoryUpdatesRanking) {
  // TODO(ellyjones): Implement.
}

TEST_F(ShareRankingTest, DISABLED_NoPersistDoesNotPersist) {
  // TODO(ellyjones): Implement.
}

TEST_F(ShareRankingTest, DISABLED_RecentHistoryUpdatesRanking) {
  // TODO(ellyjones): Implement.
}

}  // namespace sharing
