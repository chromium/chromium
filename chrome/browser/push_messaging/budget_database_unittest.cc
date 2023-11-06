// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/budget_database.h"

#include <math.h>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/push_messaging/budget.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// These values mirror the defaults in budget_database.cc
const double kDefaultExpirationInDays = 4;
const double kMaxDailyBudget = 12;

const double kEngagement = 25;

const char kTestOrigin[] = "https://example.com";

}  // namespace

class BudgetDatabaseTest : public ::testing::Test {
 public:
  BudgetDatabaseTest()
      : success_(false),
        db_(&profile_),
        origin_(url::Origin::Create(GURL(kTestOrigin))) {}

  void WriteBudgetComplete(base::OnceClosure run_loop_closure, bool success) {
    success_ = success;
    std::move(run_loop_closure).Run();
  }

  // Spend budget for the origin.
  bool SpendBudget(double amount) {
    base::RunLoop run_loop;
    db_.SpendBudget(
        origin(),
        base::BindOnce(&BudgetDatabaseTest::WriteBudgetComplete,
                       base::Unretained(this), run_loop.QuitClosure()),
        amount);
    run_loop.Run();
    return success_;
  }

  void GetBudgetDetailsComplete(base::OnceClosure run_loop_closure,
                                std::vector<BudgetState> predictions) {
    success_ = !predictions.empty();
    prediction_.swap(predictions);
    std::move(run_loop_closure).Run();
  }

  // Get the full set of budget predictions for the origin.
  void GetBudgetDetails() {
    base::RunLoop run_loop;
    db_.GetBudgetDetails(
        origin(),
        base::BindOnce(&BudgetDatabaseTest::GetBudgetDetailsComplete,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  Profile* profile() { return &profile_; }
  BudgetDatabase* database() { return &db_; }
  const url::Origin& origin() const { return origin_; }

  // Setup a test clock so that the tests can control time.
  base::SimpleTestClock* SetClockForTesting() {
    base::SimpleTestClock* clock = new base::SimpleTestClock();
    db_.SetClockForTesting(base::WrapUnique(clock));
    return clock;
  }

  void SetSiteEngagementScore(double score) {
    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(&profile_);
    service->ResetBaseScoreForURL(GURL(kTestOrigin), score);
  }

 protected:
  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }
  bool success_;
  std::vector<BudgetState> prediction_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  BudgetDatabase db_;
  base::HistogramTester histogram_tester_;
  const url::Origin origin_;
};

TEST_F(BudgetDatabaseTest, GetBudgetNoBudgetOrSES) {
  GetBudgetDetails();
  ASSERT_TRUE(success_);
  ASSERT_EQ(2U, prediction_.size());
  EXPECT_EQ(0, prediction_[0].budget_at);
}

TEST_F(BudgetDatabaseTest, AddEngagementBudgetTest) {
  base::SimpleTestClock* clock = SetClockForTesting();
  base::Time expiration_time =
      clock->Now() + base::Days(kDefaultExpirationInDays);

  // Set the default site engagement.
  SetSiteEngagementScore(kEngagement);

  // The budget should include kDefaultExpirationInDays days worth of
  // engagement.
  double daily_budget =
      kMaxDailyBudget *
      (kEngagement / site_engagement::SiteEngagementScore::kMaxPoints);
  GetBudgetDetails();
  ASSERT_TRUE(success_);
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_DOUBLE_EQ(daily_budget * kDefaultExpirationInDays,
                   prediction_[0].budget_at);
  ASSERT_EQ(0, prediction_[1].budget_at);
  ASSERT_EQ(expiration_time.InMillisecondsFSinceUnixEpoch(),
            prediction_[1].time);

  // Advance time 1 day and add more engagement budget.
  clock->Advance(base::Days(1));
  GetBudgetDetails();

  // The budget should now have 1 full share plus 1 daily budget.
  ASSERT_TRUE(success_);
  ASSERT_EQ(3U, prediction_.size());
  ASSERT_DOUBLE_EQ(daily_budget * (kDefaultExpirationInDays + 1),
                   prediction_[0].budget_at);
  ASSERT_DOUBLE_EQ(daily_budget, prediction_[1].budget_at);
  ASSERT_EQ(expiration_time.InMillisecondsFSinceUnixEpoch(),
            prediction_[1].time);
  ASSERT_DOUBLE_EQ(0, prediction_[2].budget_at);
  ASSERT_EQ((expiration_time + base::Days(1)).InMillisecondsFSinceUnixEpoch(),
            prediction_[2].time);

  // Advance time by 59 minutes and check that no engagement budget is added
  // since budget should only be added for > 1 hour increments.
  clock->Advance(base::Minutes(59));
  GetBudgetDetails();

  // The budget should be the same as before the attempted add.
  ASSERT_TRUE(success_);
  ASSERT_EQ(3U, prediction_.size());
  ASSERT_DOUBLE_EQ(daily_budget * (kDefaultExpirationInDays + 1),
                   prediction_[0].budget_at);
}

TEST_F(BudgetDatabaseTest, SpendBudgetTest) {
  base::SimpleTestClock* clock = SetClockForTesting();

  // Set the default site engagement.
  SetSiteEngagementScore(kEngagement);

  // Intialize the budget with several chunks.
  GetBudgetDetails();
  clock->Advance(base::Days(1));
  GetBudgetDetails();
  clock->Advance(base::Days(1));
  GetBudgetDetails();

  // Spend an amount of budget less than the daily budget.
  ASSERT_TRUE(SpendBudget(1));
  GetBudgetDetails();

  // There should still be three chunks of budget of size daily_budget-1,
  // daily_budget, and kDefaultExpirationInDays * daily_budget.
  double daily_budget =
      kMaxDailyBudget *
      (kEngagement / site_engagement::SiteEngagementScore::kMaxPoints);
  ASSERT_EQ(4U, prediction_.size());
  ASSERT_DOUBLE_EQ((2 + kDefaultExpirationInDays) * daily_budget - 1,
                   prediction_[0].budget_at);
  ASSERT_DOUBLE_EQ(daily_budget * 2, prediction_[1].budget_at);
  ASSERT_DOUBLE_EQ(daily_budget, prediction_[2].budget_at);
  ASSERT_DOUBLE_EQ(0, prediction_[3].budget_at);

  // Now spend enough that it will use up the rest of the first chunk and all of
  // the second chunk, but not all of the third chunk.
  ASSERT_TRUE(SpendBudget((1 + kDefaultExpirationInDays) * daily_budget));
  GetBudgetDetails();
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_DOUBLE_EQ(daily_budget - 1, prediction_[0].budget_at);

  // Validate that the code returns false if SpendBudget tries to spend more
  // budget than the origin has.
  EXPECT_FALSE(SpendBudget(kEngagement));
  GetBudgetDetails();
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_DOUBLE_EQ(daily_budget - 1, prediction_[0].budget_at);

  // Advance time until the last remaining chunk should be expired, then query
  // for the full engagement worth of budget.
  clock->Advance(base::Days(kDefaultExpirationInDays + 1));
  EXPECT_TRUE(SpendBudget(daily_budget * kDefaultExpirationInDays));
}

// There are times when a device's clock could move backwards in time, either
// due to hardware issues or user actions. Test here to make sure that even if
// time goes backwards and then forwards again, the origin isn't granted extra
// budget.
TEST_F(BudgetDatabaseTest, GetBudgetNegativeTime) {
  base::SimpleTestClock* clock = SetClockForTesting();

  // Set the default site engagement.
  SetSiteEngagementScore(kEngagement);

  // Initialize the budget with two chunks.
  GetBudgetDetails();
  clock->Advance(base::Days(1));
  GetBudgetDetails();

  // Save off the budget total.
  ASSERT_EQ(3U, prediction_.size());
  double budget = prediction_[0].budget_at;

  // Move the clock backwards in time to before the budget awards.
  clock->SetNow(clock->Now() - base::Days(5));

  // Make sure the budget is the same.
  GetBudgetDetails();
  ASSERT_EQ(3U, prediction_.size());
  ASSERT_EQ(budget, prediction_[0].budget_at);

  // Now move the clock back to the original time and check that no extra budget
  // is awarded.
  clock->SetNow(clock->Now() + base::Days(5));
  GetBudgetDetails();
  ASSERT_EQ(3U, prediction_.size());
  ASSERT_EQ(budget, prediction_[0].budget_at);
}

TEST_F(BudgetDatabaseTest, DefaultSiteEngagementInIncognitoProfile) {
  TestingProfile second_profile;
  Profile* second_profile_incognito =
      second_profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // Create a second BudgetDatabase instance for the off-the-record version of
  // a second profile. This will not have been influenced by the |profile_|.
  std::unique_ptr<BudgetDatabase> second_database =
      std::make_unique<BudgetDatabase>(second_profile_incognito);

  ASSERT_FALSE(profile()->IsOffTheRecord());
  ASSERT_FALSE(second_profile.IsOffTheRecord());
  ASSERT_TRUE(second_profile_incognito->IsOffTheRecord());

  // The Site Engagement Score considered by an Incognito profile must be equal
  // to the score considered in a regular profile visting a page for the first
  // time. This may grant a small amount of budget, but does mean that Incognito
  // mode cannot be detected through the Budget API.
  EXPECT_EQ(database()->GetSiteEngagementScoreForOrigin(origin()),
            second_database->GetSiteEngagementScoreForOrigin(origin()));
}
