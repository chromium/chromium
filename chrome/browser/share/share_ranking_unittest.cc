// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_ranking.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
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

  void ConfigureDefaultInitialRanking() {
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
  }

  std::optional<ShareRanking::Ranking> RankSync(
      ShareHistory* history,
      const std::vector<std::string>& available,
      const std::string& type = "type",
      int fold = 4,
      int length = 4,
      bool persist = true) {
    base::RunLoop loop;
    std::optional<ShareRanking::Ranking> ranking;
    auto callback =
        base::BindLambdaForTesting([&](std::optional<ShareRanking::Ranking> r) {
          ranking = r;
          loop.Quit();
        });

    backing_db()->QueueGetResult(true);
    db()->Rank(history, type, available, fold, length, persist,
               std::move(callback));
    loop.Run();

    return ranking;
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
  raw_ptr<leveldb_proto::test::FakeDB<proto::ShareRanking>> backing_db_ =
      nullptr;
};

// The "easy case": the existing usage counts are the same as the current
// ranking, and every app in the ranking is available, so the new ranking and
// the displayed ranking should both be the same as the current ranking, modulo
// the addition of the More target.
TEST(ShareRankingStaticTest, CountsMatchOldRanking) {
  std::map<std::string, int> history = {
      {"foo", 3},
      {"bar", 2},
      {"baz", 1},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "baz"};

  ShareRanking::ComputeRanking(history, history, current, {"foo", "bar", "baz"},
                               4, 4, &displayed, &persisted);

  ShareRanking::Ranking expected_displayed{"foo", "bar", "baz", "$more"};
  EXPECT_EQ(displayed, expected_displayed);
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
                               {"foo", "quxx", "baz", "blit"}, 4, 4, &displayed,
                               &persisted);
  ShareRanking::Ranking expected_displayed{
      "foo",
      "quxx",
      "baz",
      "$more",
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
                               {"foo", "bar", "quxx", "baz", "blit"}, 5, 5,
                               &displayed, &persisted);
  ShareRanking::Ranking expected_displayed{
      "foo", "bar", "baz", "blit", "$more",
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
                               {"foo", "bar", "quxx", "baz", "blit"}, 5, 5,
                               &displayed, &persisted);
  ShareRanking::Ranking expected_displayed{"foo", "bar", "baz", "blit",
                                           "$more"};
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
                               3, 3, &displayed, &persisted);

  std::vector<std::string> expected_displayed{"foo", "bar",
                                              ShareRanking::kMoreTarget};

  ASSERT_EQ(displayed.size(), expected_displayed.size());
  ASSERT_EQ(persisted.size(), current.size());
  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, current);
}

TEST_F(ShareRankingStaticTest, PromotedTargetIsVisible) {
  // Regression test for <https://crbug.com/1240355>: in "fix more" mode, the
  // last slot on the screen is actually unusable for display since it gets
  // replaced with the "More" option that invokes the system share hub. The
  // logic to promote a target into being visible should therefore not promote
  // targets into this slot in the ranking.
  std::map<std::string, int> history = {
      {"bar", 2},
      {"foo", 3},
      {"baz", 10},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "baz"};

  ShareRanking::ComputeRanking(history, history, current, {"foo", "bar", "baz"},
                               3, 3, &displayed, &persisted);

  std::vector<std::string> expected_displayed{"foo", "baz",
                                              ShareRanking::kMoreTarget};
  std::vector<std::string> expected_persisted{"foo", "baz", "bar"};

  ASSERT_EQ(displayed.size(), expected_displayed.size());
  ASSERT_EQ(persisted.size(), current.size());
  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, expected_persisted);
}

TEST_F(ShareRankingStaticTest, UsedAppNotPresentInRanking) {
  std::map<std::string, int> history = {
      {"bar", 2},
      {"foo", 3},
      {"baz", 10},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"foo", "bar", "quxx"};

  ShareRanking::ComputeRanking(history, history, current, {"foo", "bar", "baz"},
                               3, 3, &displayed, &persisted);

  // Since length is 3 only 2 actual items show, with the visible one with the
  // lowest usage (bar) replaced with baz. Since baz was new (not in the old
  // ranking) it should have been stuck at the end of the ranking, then swapped
  // with bar, leaving bar at the end.
  std::vector<std::string> expected_displayed{"foo", "baz",
                                              ShareRanking::kMoreTarget};
  std::vector<std::string> expected_persisted{"foo", "baz", "quxx", "bar"};

  ASSERT_EQ(displayed.size(), expected_displayed.size());
  ASSERT_EQ(persisted.size(), expected_persisted.size());
  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, expected_persisted);
}

TEST_F(ShareRankingStaticTest, LowestUsageItemSwappedIgnoringDisplayOrder) {
  std::map<std::string, int> history = {
      {"bar", 2},
      {"foo", 3},
      {"baz", 4},
      {"quxx", 5},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::Ranking current{"bar", "foo", "baz"};

  ShareRanking::ComputeRanking(history, history, current,
                               {"foo", "bar", "baz", "quxx"}, 3, 3, &displayed,
                               &persisted);

  ShareRanking::Ranking expected_displayed{"quxx", "foo", "$more"};
  ShareRanking::Ranking expected_persisted{"quxx", "foo", "baz", "bar"};
  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, expected_persisted);
}

TEST_F(ShareRankingStaticTest, SystemAppsReplaceUnavailableInRankedOrder) {
  // This test ensures that unavailable app "slots" in the old ranking are
  // replaced by available apps in the order of the old ranking, *not* in the
  // order supplied by the system.
  const ShareRanking::Ranking current{
      "aaa", "bbb", "ccc", "ddd", "eee", "fff", "ggg", "hhh", "iii", "jjj",
  };
  const std::vector<std::string> available{
      "iii", "ggg", "eee", "ccc", "aaa",
  };

  ShareRanking::Ranking displayed, persisted;

  ShareRanking::ComputeRanking({}, {}, current, available, 4, 4, &displayed,
                               &persisted);

  ShareRanking::Ranking expected_displayed{"aaa", "eee", "ccc", "$more"};

  EXPECT_EQ(displayed, expected_displayed);
}

TEST_F(ShareRankingStaticTest, NotEnoughPreferredApps) {
  const ShareRanking::Ranking current{
      "aaa", "bbb", "ccc", "ddd", "eee", "fff", "ggg", "hhh", "iii", "jjj",
  };
  const std::vector<std::string> available{
      "zzz", "yyy", "xxx", "ccc", "aaa",
  };

  ShareRanking::Ranking displayed, persisted;

  ShareRanking::ComputeRanking({}, {}, current, available, 4, 4, &displayed,
                               &persisted);

  ShareRanking::Ranking expected_displayed{"aaa", "zzz", "ccc", "$more"};

  EXPECT_EQ(displayed, expected_displayed);
}

TEST_F(ShareRankingStaticTest, SwapAboveFold) {
  const ShareRanking::Ranking current{
      "aaa", "bbb", "ccc", "ddd", "eee", "fff", "ggg",
  };
  const std::vector<std::string> available{
      "aaa", "bbb", "ccc", "ddd", "eee", "fff", "ggg", "hhh", "iii", "jjj",
  };

  std::map<std::string, int> history = {
      {"bbb", 1},
      {"ccc", 1},
      {"ddd", 1},
      {"eee", 1},
  };

  ShareRanking::Ranking displayed, persisted;
  ShareRanking::ComputeRanking(history, history, current, available, 4, 7,
                               &displayed, &persisted);

  ShareRanking::Ranking expected_displayed{
      "eee", "bbb", "ccc", "ddd", "aaa", "fff", "$more",
  };

  EXPECT_EQ(expected_displayed, displayed);
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
                               4, 4, &displayed, &persisted);

  // Note that since "abc" isn't available on the system, it won't appear in the
  // displayed ranking, but the persisted ranking should still get updated.
  ShareRanking::Ranking expected_displayed{"foo", "bar", "baz", "$more"};
  ShareRanking::Ranking expected_persisted{"foo", "bar", "abc", "baz"};

  EXPECT_EQ(displayed, expected_displayed);
  EXPECT_EQ(persisted, expected_persisted);
}

TEST_F(ShareRankingTest, InitialStateNoHistory) {
  ConfigureDefaultInitialRanking();

  FakeShareHistory history;
  history.set_history({});

  auto ranking = RankSync(&history, {"aaa", "ccc", "eee", "ggg", "iii"});

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
  // TODO(crbug.com/40191160): Implement.
}

TEST_F(ShareRankingTest, DISABLED_NoPersistDoesNotPersist) {
  // TODO(crbug.com/40191160): Implement.
}

TEST_F(ShareRankingTest, DISABLED_RecentHistoryUpdatesRanking) {
  // TODO(crbug.com/40191160): Implement.
}

TEST_F(ShareRankingTest, ClearClearsDatabase) {
  ConfigureDefaultInitialRanking();

  FakeShareHistory history;
  history.set_history({{"iii", 10}, {"aaa", 2}, {"ccc", 1}});

  auto pre_ranking = RankSync(&history, {"aaa", "ccc", "eee", "ggg", "iii"});

  ASSERT_TRUE(pre_ranking);
  EXPECT_EQ(*pre_ranking,
            std::vector<std::string>({"aaa", "iii", "ccc", "$more"}));

  backing_db()->UpdateCallback(true);

  auto pre_entries = backing_entries()->at("type");
  // The stored ranking should be identical to the default ranking we injected
  // above, since there's no history to influence it.
  EXPECT_EQ(pre_entries.targets().at(0), "aaa");
  EXPECT_EQ(pre_entries.targets().at(1), "iii");
  EXPECT_EQ(pre_entries.targets().at(2), "ccc");

  history.set_history({});
  db()->Clear();

  // After clearing both the fake history and the ranking, we should get the
  // initial ranking back, with the (present on system) "eee" app swapped in for
  // "bbb", but "bbb" persisted to disk.
  auto post_ranking = RankSync(&history, {"aaa", "ccc", "eee", "ggg", "iii"});
  ASSERT_TRUE(post_ranking);
  EXPECT_EQ(*post_ranking,
            std::vector<std::string>({"aaa", "eee", "ccc", "$more"}));

  auto post_entries = backing_entries()->at("type");
  EXPECT_EQ(post_entries.targets().at(0), "aaa");
  EXPECT_EQ(post_entries.targets().at(1), "bbb");
  EXPECT_EQ(post_entries.targets().at(2), "ccc");
}

}  // namespace sharing
