// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/session_restore_policy.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

// Delegate that exposes testing seams for testing SessionRestorePolicy.
class TestDelegate : public SessionRestorePolicy::Delegate {
 public:
  explicit TestDelegate(base::TickClock* clock) : clock_(clock) {}

  ~TestDelegate() override {}

  size_t GetNumberOfCores() const override { return number_of_cores_; }
  size_t GetFreeMemoryMiB() const override { return free_memory_mb_; }
  base::TimeTicks NowTicks() const override { return clock_->NowTicks(); }

  size_t GetSiteEngagementScore(
      content::WebContents* unused_contents) const override {
    return site_engagement_score_;
  }

  void SetNumberOfCores(size_t number_of_cores) {
    number_of_cores_ = number_of_cores;
  }

  void SetFreeMemoryMiB(size_t free_memory_mb) {
    free_memory_mb_ = free_memory_mb;
  }

  void SetSiteEngagementScore(size_t site_engagement_score) {
    site_engagement_score_ = site_engagement_score;
  }

 private:
  size_t number_of_cores_ = 1;
  size_t free_memory_mb_ = 0;
  base::TickClock* clock_ = nullptr;
  size_t site_engagement_score_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class LenientTabScoreChangeMock {
 public:
  LenientTabScoreChangeMock() = default;
  ~LenientTabScoreChangeMock() = default;

  MOCK_METHOD2(NotifyTabScoreChanged, void(content::WebContents*, float));
};
using TabScoreChangeMock = ::testing::StrictMock<LenientTabScoreChangeMock>;

// Exposes testing functions on SessionRestorePolicy.
class TestSessionRestorePolicy : public SessionRestorePolicy {
 public:
  using SessionRestorePolicy::CalculateAgeScore;
  using SessionRestorePolicy::CalculateSimultaneousTabLoads;
  using SessionRestorePolicy::ScoreTab;
  using SessionRestorePolicy::SetTabLoadsStartedForTesting;
  using SessionRestorePolicy::TabData;
  using SessionRestorePolicy::UpdateSiteEngagementScoreForTesting;

  // Expose some member variables.
  using SessionRestorePolicy::tab_data_;

  // Expose parameters.
  using SessionRestorePolicy::cores_per_simultaneous_tab_load_;
  using SessionRestorePolicy::max_simultaneous_tab_loads_;
  using SessionRestorePolicy::max_tabs_to_restore_;
  using SessionRestorePolicy::max_time_since_last_use_to_restore_;
  using SessionRestorePolicy::mb_free_memory_per_tab_to_restore_;
  using SessionRestorePolicy::min_simultaneous_tab_loads_;
  using SessionRestorePolicy::min_site_engagement_to_restore_;
  using SessionRestorePolicy::min_tabs_to_restore_;
  using SessionRestorePolicy::simultaneous_tab_loads_;

  TestSessionRestorePolicy(bool policy_enabled, const Delegate* delegate)
      : SessionRestorePolicy(policy_enabled, delegate) {}

  ~TestSessionRestorePolicy() override {}

  using RescoreTabCallback =
      base::RepeatingCallback<bool(content::WebContents*, TabData*)>;

  void SetRescoreTabCallback(RescoreTabCallback rescore_tab_callback) {
    rescore_tab_callback_ = rescore_tab_callback;
  }

  bool RescoreTabAfterDataLoaded(content::WebContents* contents,
                                 TabData* tab_data) override {
    // Invoke the callback if one is provided.
    if (!rescore_tab_callback_.is_null())
      return rescore_tab_callback_.Run(contents, tab_data);
    // Otherwise defer to the default implementation.
    return SessionRestorePolicy::RescoreTabAfterDataLoaded(contents, tab_data);
  }

  float GetTabScore(content::WebContents* contents) const {
    auto it = tab_data_.find(contents);
    return it->second.score;
  }

 private:
  RescoreTabCallback rescore_tab_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestSessionRestorePolicy);
};

}  // namespace

class SessionRestorePolicyTest : public testing::ChromeTestHarnessWithLocalDB {
 public:
  SessionRestorePolicyTest() : delegate_(&clock_) {}

  ~SessionRestorePolicyTest() override {}

  void SetUp() override {
    testing::ChromeTestHarnessWithLocalDB::SetUp();

    // Set some reasonable delegate constants.
    delegate_.SetNumberOfCores(4);
    delegate_.SetFreeMemoryMiB(1024);
    delegate_.SetSiteEngagementScore(30);

    // Put the clock in the future so that we can LastActiveTimes in the past.
    clock_.Advance(base::TimeDelta::FromDays(1));

    contents1_ = CreateTestWebContents();
    contents2_ = CreateTestWebContents();
    contents3_ = CreateTestWebContents();

    content::WebContentsTester::For(contents1_.get())
        ->SetLastActiveTime(clock_.NowTicks() - base::TimeDelta::FromHours(1));
    content::WebContentsTester::For(contents2_.get())
        ->SetLastActiveTime(clock_.NowTicks() - base::TimeDelta::FromHours(2));
    content::WebContentsTester::For(contents3_.get())
        ->SetLastActiveTime(clock_.NowTicks() - base::TimeDelta::FromHours(3));
  }

  void TearDown() override {
    // The WebContents must be deleted before the test harness deletes the
    // RenderProcessHost.
    contents1_.reset();
    contents2_.reset();
    contents3_.reset();

    testing::ChromeTestHarnessWithLocalDB::TearDown();
  }

  void CreatePolicy(bool policy_enabled) {
    policy_ =
        std::make_unique<TestSessionRestorePolicy>(policy_enabled, &delegate_);

    // Set some reasonable initial parameters.
    policy_->min_simultaneous_tab_loads_ = 1;
    policy_->max_simultaneous_tab_loads_ = 4;
    policy_->cores_per_simultaneous_tab_load_ = 2;
    policy_->min_tabs_to_restore_ = 2;
    policy_->max_tabs_to_restore_ = 30;
    policy_->mb_free_memory_per_tab_to_restore_ = 150;
    policy_->max_time_since_last_use_to_restore_ =
        base::TimeDelta::FromHours(6);
    policy_->min_site_engagement_to_restore_ = 15;

    // Ensure the simultaneous tab loads is properly calculated wrt the above
    // parameters.
    policy_->CalculateSimultaneousTabLoadsForTesting();

    policy_->SetTabScoreChangedCallback(base::BindRepeating(
        &TabScoreChangeMock::NotifyTabScoreChanged, base::Unretained(&mock_)));
    policy_->AddTabForScoring(contents1_.get());
    policy_->AddTabForScoring(contents2_.get());
    policy_->AddTabForScoring(contents3_.get());
  }

  void WaitForFinalTabScores() {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_, NotifyTabScoreChanged(nullptr, 0.0))
        .WillOnce(::testing::Invoke(
            [&run_loop](content::WebContents*, float) { run_loop.Quit(); }));
    run_loop.Run();
  }

 protected:
  base::SimpleTestTickClock clock_;
  TestDelegate delegate_;

  TabScoreChangeMock mock_;
  std::unique_ptr<TestSessionRestorePolicy> policy_;

  std::unique_ptr<content::WebContents> contents1_;
  std::unique_ptr<content::WebContents> contents2_;
  std::unique_ptr<content::WebContents> contents3_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionRestorePolicyTest);
};

TEST_F(SessionRestorePolicyTest, CalculateSimultaneousTabLoads) {
  using TSRP = TestSessionRestorePolicy;

  // Test the minimum is enforced.
  EXPECT_EQ(10u, TSRP::CalculateSimultaneousTabLoads(10 /* min */, 20 /* max */,
                                                     1 /* cores_per_load */,
                                                     1 /* cores */));

  // Test the maximum is enforced.
  EXPECT_EQ(20u, TSRP::CalculateSimultaneousTabLoads(10 /* min */, 20 /* max */,
                                                     1 /* cores_per_load */,
                                                     30 /* cores */));

  // Test the per-core calculation is correct.
  EXPECT_EQ(15u, TSRP::CalculateSimultaneousTabLoads(10 /* min */, 20 /* max */,
                                                     1 /* cores_per_load */,
                                                     15 /* cores */));
  EXPECT_EQ(15u, TSRP::CalculateSimultaneousTabLoads(10 /* min */, 20 /* max */,
                                                     2 /* cores_per_load */,
                                                     30 /* cores */));

  // If no per-core is specified then max is returned.
  EXPECT_EQ(5u, TSRP::CalculateSimultaneousTabLoads(1 /* min */, 5 /* max */,
                                                    0 /* cores_per_load */,
                                                    10 /* cores */));

  // If no per-core and no max is applied, then "max" is returned.
  EXPECT_EQ(
      std::numeric_limits<size_t>::max(),
      TSRP::CalculateSimultaneousTabLoads(
          3 /* min */, 0 /* max */, 0 /* cores_per_load */, 4 /* cores */));
}

TEST_F(SessionRestorePolicyTest, ShouldLoadFeatureEnabled) {
  CreatePolicy(true);
  EXPECT_TRUE(policy_->policy_enabled());
  EXPECT_EQ(2u, policy_->simultaneous_tab_loads());

  WaitForFinalTabScores();

  // By default all the tabs should be loadable.
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_TRUE(policy_->ShouldLoad(contents2_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_TRUE(policy_->ShouldLoad(contents3_.get()));
  policy_->NotifyTabLoadStarted();

  // Reset and set a maximum number of tabs to load policy.
  policy_->SetTabLoadsStartedForTesting(0);
  policy_->max_tabs_to_restore_ = 2;
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_TRUE(policy_->ShouldLoad(contents2_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_FALSE(policy_->ShouldLoad(contents3_.get()));

  // Disable the number of tab load limits entirely.
  policy_->min_tabs_to_restore_ = 0;
  policy_->max_tabs_to_restore_ = 0;

  // Reset and impose a memory policy.
  policy_->SetTabLoadsStartedForTesting(0);
  constexpr size_t kFreeMemoryLimit = 150;
  policy_->mb_free_memory_per_tab_to_restore_ = kFreeMemoryLimit;
  delegate_.SetFreeMemoryMiB(kFreeMemoryLimit);
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  delegate_.SetFreeMemoryMiB(kFreeMemoryLimit - 1);
  EXPECT_FALSE(policy_->ShouldLoad(contents2_.get()));
  delegate_.SetFreeMemoryMiB(kFreeMemoryLimit + 1);
  EXPECT_TRUE(policy_->ShouldLoad(contents3_.get()));
  policy_->NotifyTabLoadStarted();

  // Disable memory limits to not interfere with later tests.
  policy_->mb_free_memory_per_tab_to_restore_ = 0;

  // Reset and impose a max time since use policy. The contents have ages of 1,
  // 2 and 3 hours respectively.
  policy_->SetTabLoadsStartedForTesting(0);
  policy_->max_time_since_last_use_to_restore_ =
      base::TimeDelta::FromMinutes(90);
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_FALSE(policy_->ShouldLoad(contents2_.get()));
  EXPECT_FALSE(policy_->ShouldLoad(contents3_.get()));

  // Disable the age limits entirely.
  policy_->max_time_since_last_use_to_restore_ = base::TimeDelta();

  // Reset and impose a site engagement policy.
  policy_->SetTabLoadsStartedForTesting(0);
  constexpr size_t kEngagementLimit = 15;
  policy_->min_site_engagement_to_restore_ = kEngagementLimit;
  policy_->UpdateSiteEngagementScoreForTesting(contents1_.get(),
                                               kEngagementLimit + 1);
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->UpdateSiteEngagementScoreForTesting(contents1_.get(),
                                               kEngagementLimit);
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->UpdateSiteEngagementScoreForTesting(contents1_.get(),
                                               kEngagementLimit - 1);
  EXPECT_FALSE(policy_->ShouldLoad(contents1_.get()));
}

TEST_F(SessionRestorePolicyTest, ShouldLoadFeatureDisabled) {
  CreatePolicy(false);
  EXPECT_FALSE(policy_->policy_enabled());
  EXPECT_EQ(std::numeric_limits<size_t>::max(),
            policy_->simultaneous_tab_loads());

  WaitForFinalTabScores();

  // Set everything aggressive so it would return false if the feature was
  // enabled.
  policy_->min_tabs_to_restore_ = 0;
  policy_->max_tabs_to_restore_ = 1;
  policy_->mb_free_memory_per_tab_to_restore_ = 1024;
  policy_->max_time_since_last_use_to_restore_ =
      base::TimeDelta::FromMinutes(1);
  policy_->min_site_engagement_to_restore_ = 100;

  // Make the system look like its effectively out of memory as well.
  delegate_.SetFreeMemoryMiB(1);

  // Everything should still be allowed to load, as the policy engine is
  // disabled.
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_TRUE(policy_->ShouldLoad(contents2_.get()));
  policy_->NotifyTabLoadStarted();
  EXPECT_TRUE(policy_->ShouldLoad(contents3_.get()));
  policy_->NotifyTabLoadStarted();
}

TEST_F(SessionRestorePolicyTest, ShouldLoadBackgroundData) {
  using TabData = TestSessionRestorePolicy::TabData;

  CreatePolicy(true);
  EXPECT_TRUE(policy_->policy_enabled());
  EXPECT_EQ(2u, policy_->simultaneous_tab_loads());

  WaitForFinalTabScores();

  // Disable other limit mechanisms.
  policy_->min_tabs_to_restore_ = 0;
  policy_->max_tabs_to_restore_ = 0;
  policy_->mb_free_memory_per_tab_to_restore_ = 0;
  policy_->max_time_since_last_use_to_restore_ = base::TimeDelta();

  constexpr size_t kEngagementLimit = 15;
  policy_->min_site_engagement_to_restore_ = kEngagementLimit;
  policy_->UpdateSiteEngagementScoreForTesting(contents1_.get(),
                                               kEngagementLimit + 1);
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->UpdateSiteEngagementScoreForTesting(contents1_.get(),
                                               kEngagementLimit);
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
  policy_->UpdateSiteEngagementScoreForTesting(contents1_.get(),
                                               kEngagementLimit - 1);
  EXPECT_FALSE(policy_->ShouldLoad(contents1_.get()));

  // Mark the tab as using background communication mechanisms, and expect the
  // site engagement policy to no longer be applied.
  TabData* tab_data = &policy_->tab_data_[contents1_.get()];
  tab_data->used_in_bg = true;
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
}

TEST_F(SessionRestorePolicyTest, MultipleAllTabsDoneCallbacks) {
  CreatePolicy(true);
  WaitForFinalTabScores();

  // Another "all tabs scored" notification should be sent after more tabs
  // are added to the policy engine.
  std::unique_ptr<content::WebContents> contents4 = CreateTestWebContents();
  std::unique_ptr<content::WebContents> contents5 = CreateTestWebContents();
  policy_->AddTabForScoring(contents4.get());
  policy_->AddTabForScoring(contents5.get());
  WaitForFinalTabScores();
}

TEST_F(SessionRestorePolicyTest, CalculateAgeScore) {
  using TabData = TestSessionRestorePolicy::TabData;
  constexpr int kMonthInSeconds = 60 * 60 * 24 * 31;

  // Generate a bunch of random tab ages.
  std::vector<TabData> tab_data;
  tab_data.resize(1000);

  // Generate some known edge cases.
  size_t i = 0;
  tab_data[i++].last_active = base::TimeDelta::FromMilliseconds(-1001);
  tab_data[i++].last_active = base::TimeDelta::FromMilliseconds(-1000);
  tab_data[i++].last_active = base::TimeDelta::FromMilliseconds(-999);
  tab_data[i++].last_active = base::TimeDelta::FromMilliseconds(-500);
  tab_data[i++].last_active = base::TimeDelta::FromMilliseconds(0);
  tab_data[i++].last_active = base::TimeDelta::FromMilliseconds(500);
  tab_data[i++].last_active = base::TimeDelta::FromMilliseconds(999);
  tab_data[i++].last_active = base::TimeDelta::FromMilliseconds(1000);
  tab_data[i++].last_active = base::TimeDelta::FromMilliseconds(1001);

  // Generate a logarithmic selection of ages to test the whole range.
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(-1000000);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(-100000);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(-10000);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(-1000);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(-100);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(-10);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(10);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(100);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(1000);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(10000);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(100000);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(1000000);
  tab_data[i++].last_active = base::TimeDelta::FromSeconds(10000000);

  // Generate a bunch more random ages.
  for (; i < tab_data.size(); ++i) {
    tab_data[i].last_active = base::TimeDelta::FromSeconds(
        base::RandInt(-kMonthInSeconds, kMonthInSeconds));
  }

  // Calculate the tab scores.
  for (i = 0; i < tab_data.size(); ++i) {
    tab_data[i].score =
        TestSessionRestorePolicy::CalculateAgeScore(&tab_data[i]);
  }

  // Sort tabs by increasing last active time.
  std::sort(tab_data.begin(), tab_data.end(),
            [](const TabData& td1, const TabData& td2) {
              return td1.last_active < td2.last_active;
            });

  // The scores should be in decreasing order (>= is necessary because some
  // last active times collapse to the same score).
  for (i = 1; i < tab_data.size(); ++i)
    ASSERT_GE(tab_data[i - 1].score, tab_data[i].score);
}

TEST_F(SessionRestorePolicyTest, ScoreTab) {
  using TabData = TestSessionRestorePolicy::TabData;

  TabData td_bg;
  td_bg.used_in_bg = true;
  td_bg.last_active = base::TimeDelta::FromDays(30);
  EXPECT_TRUE(TestSessionRestorePolicy::ScoreTab(&td_bg));

  TabData td_normal_young;
  TabData td_normal_old;
  td_normal_young.last_active = base::TimeDelta::FromSeconds(1);
  td_normal_old.last_active = base::TimeDelta::FromDays(7);
  EXPECT_TRUE(TestSessionRestorePolicy::ScoreTab(&td_normal_young));
  EXPECT_TRUE(TestSessionRestorePolicy::ScoreTab(&td_normal_old));

  TabData td_internal;
  td_internal.is_internal = true;
  EXPECT_TRUE(TestSessionRestorePolicy::ScoreTab(&td_internal));

  // Check the score produces the expected ordering of tabs.
  EXPECT_LT(td_internal.score, td_normal_old.score);
  EXPECT_LT(td_normal_old.score, td_normal_young.score);
  EXPECT_LT(td_normal_young.score, td_bg.score);
}

TEST_F(SessionRestorePolicyTest, RescoringSendsNotification) {
  using TabData = TestSessionRestorePolicy::TabData;

  // Inject code that causes some tabs to receive updated scores.
  CreatePolicy(true);
  policy_->SetRescoreTabCallback(base::BindLambdaForTesting(
      [&](content::WebContents* contents, TabData* tab_data) {
        float delta = 0;
        if (contents == contents1_.get())
          delta = 1.0;
        else if (contents == contents2_.get())
          delta = 2.0;
        tab_data->score += delta;
        return delta != 0;
      }));

  // Get the current scores.
  float score1 = policy_->GetTabScore(contents1_.get());
  float score2 = policy_->GetTabScore(contents2_.get());

  // Expect tab score change notifications for the first 2 tabs, but not the
  // third.
  EXPECT_CALL(mock_, NotifyTabScoreChanged(contents1_.get(), score1 + 1.0));
  EXPECT_CALL(mock_, NotifyTabScoreChanged(contents2_.get(), score2 + 2.0));
  WaitForFinalTabScores();
}

}  // namespace resource_coordinator
