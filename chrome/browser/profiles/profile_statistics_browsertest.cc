// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_aggregator.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

std::set<std::string> stats_categories() {
  std::set<std::string> categories;
  categories.insert(profiles::kProfileStatisticsBrowsingHistory);
  categories.insert(profiles::kProfileStatisticsPasswords);
  categories.insert(profiles::kProfileStatisticsBookmarks);
  categories.insert(profiles::kProfileStatisticsAutofill);
  EXPECT_EQ(4u, categories.size());
  return categories;
}

bool IsProfileCategoryStatEqual(const profiles::ProfileCategoryStat& a,
                                const profiles::ProfileCategoryStat& b) {
  return a.category == b.category && a.count == b.count;
}

std::string ProfileCategoryStatToString(
    const profiles::ProfileCategoryStat& a) {
  return base::StringPrintf("category = %s, count = %d", a.category.c_str(),
                            a.count);
}

::testing::AssertionResult AssertionProfileCategoryStatEqual(
    const char* actual_expression,
    const char* expected_expression,
    const profiles::ProfileCategoryStat& actual_value,
    const profiles::ProfileCategoryStat& expected_value) {
  if (IsProfileCategoryStatEqual(actual_value, expected_value)) {
    return ::testing::AssertionSuccess();
  } else {
    return ::testing::AssertionFailure()
        << "Value of: " << actual_expression
        << "\n  Actual: " << ProfileCategoryStatToString(actual_value)
        << "\nExpected: " << expected_expression
        << "\nWhich is: " << ProfileCategoryStatToString(expected_value);
  }
}

::testing::AssertionResult AssertionProfileCategoryStatsEqual(
    const char* actual_expression,
    const char* expected_expression,
    const profiles::ProfileCategoryStats& actual_value,
    const profiles::ProfileCategoryStats& expected_value) {
  if (base::ranges::is_permutation(actual_value, expected_value,
                                   IsProfileCategoryStatEqual)) {
    return ::testing::AssertionSuccess();
  } else {
    ::testing::AssertionResult result = testing::AssertionFailure();
    result << "ProfileCategoryStats are not equal.";

    result << "\n  Actual: " << actual_expression << "\nWhich is:";
    for (const auto& value : actual_value)
      result << "\n    " << ProfileCategoryStatToString(value);

    result << "\nExpected: " << expected_expression << "\nWhich is:";
    for (const auto& value : expected_value)
      result << "\n    " << ProfileCategoryStatToString(value);

    return result;
  }
}

class ProfileStatisticsAggregatorState {
 public:
  ProfileStatisticsAggregatorState()
      : ProfileStatisticsAggregatorState(stats_categories().size()) {}

  explicit ProfileStatisticsAggregatorState(size_t required_stat_count) {
    stats_categories_ = stats_categories();
    num_of_stats_categories_ = stats_categories_.size();
    SetRequiredStatCountAndCreateRunLoop(required_stat_count);
  }

  void SetRequiredStatCountAndCreateRunLoop(size_t required_stat_count) {
    EXPECT_GE(num_of_stats_categories_, required_stat_count);
    required_stat_count_ = required_stat_count;
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void WaitForStats() {
    run_loop_->Run();
    run_loop_.reset();
  }

  profiles::ProfileCategoryStats GetStats() const { return stats_; }

  void StatsCallback(profiles::ProfileCategoryStats stats_return) {
    size_t newCount = stats_return.size();
    // If newCount is 1, then a new GatherStatistics task has started. Discard
    // the old statistics by setting oldCount to 0 in this case.
    size_t oldCount = newCount == 1u ? 0u : stats_.size();

    // Only one new statistic arrives at a time.
    EXPECT_EQ(oldCount + 1u, newCount);
    for (size_t i = 0u; i < oldCount; i++) {
      // Exisiting statistics must be the same.
      EXPECT_PRED_FORMAT2(AssertionProfileCategoryStatEqual,
                          stats_[i], stats_return[i]);
    }

    for (size_t i = 0u; i < newCount; i++) {
      // The category must be a valid category.
      EXPECT_EQ(1u, stats_categories_.count(stats_return[i].category));
      // The categories in |stats_return| must all different.
      for (size_t j = 0u; j < i; j++)
        EXPECT_NE(stats_return[i].category, stats_return[j].category);
    }
    stats_ = stats_return;

    EXPECT_GE(num_of_stats_categories_, newCount);
    if (required_stat_count_ <= newCount)
      run_loop_->Quit();
  }

 private:
  std::set<std::string> stats_categories_;
  size_t num_of_stats_categories_;
  size_t required_stat_count_;
  std::unique_ptr<base::RunLoop> run_loop_;

  profiles::ProfileCategoryStats stats_;
};

}  // namespace

class ProfileStatisticsBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  // Use TestPasswordStore to remove a possible race. Normally
                  // the PasswordStore does its database manipulation on the DB
                  // thread, which creates a possible race during navigation.
                  // Specifically the PasswordManager will ignore any forms in a
                  // page if the load from the PasswordStore has not completed.
                  ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
                      context, base::BindRepeating(
                                   &password_manager::BuildPasswordStore<
                                       content::BrowserContext,
                                       password_manager::TestPasswordStore>));
                }));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
};

using ProfileStatisticsBrowserDeathTest = ProfileStatisticsBrowserTest;

IN_PROC_BROWSER_TEST_F(ProfileStatisticsBrowserTest, GatherStatistics) {
  Profile* profile = browser()->profile();
  ASSERT_TRUE(profile);
  ProfileStatistics* profile_stat =
      ProfileStatisticsFactory::GetForProfile(profile);

  ProfileStatisticsAggregatorState state;
  profile_stat->GatherStatistics(
      base::BindRepeating(&ProfileStatisticsAggregatorState::StatsCallback,
                          base::Unretained(&state)));
  state.WaitForStats();

  profiles::ProfileCategoryStats stats = state.GetStats();
  for (const auto& stat : stats) {
      EXPECT_EQ(0, stat.count);
  }
}

IN_PROC_BROWSER_TEST_F(ProfileStatisticsBrowserTest,
                       GatherStatisticsTwoCallbacks) {
  Profile* profile = browser()->profile();
  ASSERT_TRUE(profile);
  ProfileStatistics* profile_stat =
      ProfileStatisticsFactory::GetForProfile(profile);

  ProfileStatisticsAggregatorState state1(1u);
  ProfileStatisticsAggregatorState state2;

  profile_stat->GatherStatistics(
      base::BindRepeating(&ProfileStatisticsAggregatorState::StatsCallback,
                          base::Unretained(&state1)));
  state1.WaitForStats();

  state1.SetRequiredStatCountAndCreateRunLoop(stats_categories().size());

  profile_stat->GatherStatistics(
      base::BindRepeating(&ProfileStatisticsAggregatorState::StatsCallback,
                          base::Unretained(&state2)));
  state1.WaitForStats();
  state2.WaitForStats();

  EXPECT_PRED_FORMAT2(AssertionProfileCategoryStatsEqual,
      state1.GetStats(), state2.GetStats());
}
