// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/session_restore_policy.h"

#include <algorithm>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/performance_manager/test_support/site_data_utils.h"
#endif
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/public/decorators/site_data_recorder.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_permission_controller.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

// Delegate that exposes testing seams for testing SessionRestorePolicy.
class TestDelegate : public SessionRestorePolicy::Delegate {
 public:
  explicit TestDelegate(base::TickClock* clock) : clock_(clock) {}

  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

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
  raw_ptr<base::TickClock> clock_ = nullptr;
  size_t site_engagement_score_ = 0;
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

  TestSessionRestorePolicy(const TestSessionRestorePolicy&) = delete;
  TestSessionRestorePolicy& operator=(const TestSessionRestorePolicy&) = delete;

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
    return it->second->score;
  }

 private:
  RescoreTabCallback rescore_tab_callback_;
};

}  // namespace

class SessionRestorePolicyTest : public ChromeRenderViewHostTestHarness {
 public:
  SessionRestorePolicyTest() : delegate_(&clock_) {}

  SessionRestorePolicyTest(const SessionRestorePolicyTest&) = delete;
  SessionRestorePolicyTest& operator=(const SessionRestorePolicyTest&) = delete;

  ~SessionRestorePolicyTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

#if !BUILDFLAG(IS_ANDROID)
    // Some tests requires the SiteData database to be initialized.
    site_data_harness_.SetUp();
#endif

    // Set some reasonable delegate constants.
    delegate_.SetNumberOfCores(4);
    delegate_.SetFreeMemoryMiB(1024);
    delegate_.SetSiteEngagementScore(30);

    // Put the clock in the future so that we can LastActiveTimes in the past.
    clock_.Advance(base::Days(1));

    CreateTestContents();
  }

  void TearDown() override {
#if !BUILDFLAG(IS_ANDROID)
    performance_manager::MarkWebContentsAsUnloadedInBackgroundInSiteDataDb(
        contents1_.get());
    performance_manager::MarkWebContentsAsUnloadedInBackgroundInSiteDataDb(
        contents2_.get());
    performance_manager::MarkWebContentsAsUnloadedInBackgroundInSiteDataDb(
        contents3_.get());
#endif
    if (policy_)
      policy_.reset();

    // The WebContents must be deleted before the test harness deletes the
    // RenderProcessHost.
    contents1_.reset();
    contents2_.reset();
    contents3_.reset();

    tab_for_scoring_.clear();

#if !BUILDFLAG(IS_ANDROID)
    site_data_harness_.TearDown(profile());
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateTestContents() {
    contents1_ = CreateAndInitTestWebContents(
        GURL("https://a.com"), clock_.NowTicks() - base::Hours(1));
    contents2_ = CreateAndInitTestWebContents(
        GURL("https://b.com"), clock_.NowTicks() - base::Hours(2));
    contents3_ = CreateAndInitTestWebContents(
        GURL("https://c.com"), clock_.NowTicks() - base::Hours(3));

    tab_for_scoring_ = {contents1_.get(), contents2_.get(), contents3_.get()};
  }

  std::unique_ptr<content::WebContents> CreateAndInitTestWebContents(
      const GURL& url,
      const base::TimeTicks& last_active) {
    auto contents = CreateTestWebContents();
    auto* tester = content::WebContentsTester::For(contents.get());
    tester->SetLastActiveTimeTicks(last_active);

#if !BUILDFLAG(IS_ANDROID)
    tester->NavigateAndCommit(url);
    performance_manager::MarkWebContentsAsLoadedInBackgroundInSiteDataDb(
        contents.get());
    performance_manager::ExpireSiteDataObservationWindowsForWebContents(
        contents.get());
#endif
    return contents;
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
    policy_->max_time_since_last_use_to_restore_ = base::Hours(6);
    policy_->min_site_engagement_to_restore_ = 15;

    // Ensure the simultaneous tab loads is properly calculated wrt the above
    // parameters.
    policy_->CalculateSimultaneousTabLoadsForTesting();

    policy_->SetTabScoreChangedCallback(base::BindRepeating(
        &TabScoreChangeMock::NotifyTabScoreChanged, base::Unretained(&mock_)));

    for (content::WebContents* tab : tab_for_scoring_) {
      policy_->AddTabForScoring(tab);
    }
  }

  void WaitForFinalTabScores() {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_, NotifyTabScoreChanged(nullptr, 0.0))
        .WillOnce(::testing::Invoke(
            [&run_loop](content::WebContents*, float) { run_loop.Quit(); }));
    run_loop.Run();
  }

  void AddExtraTabForScoring(content::WebContents* contents) {
    tab_for_scoring_.push_back(contents);
  }

 protected:
  base::SimpleTestTickClock clock_;
  TestDelegate delegate_;

#if !BUILDFLAG(IS_ANDROID)
  performance_manager::SiteDataTestHarness site_data_harness_;
#endif

  TabScoreChangeMock mock_;
  std::unique_ptr<TestSessionRestorePolicy> policy_;

  std::unique_ptr<content::WebContents> contents1_;
  std::unique_ptr<content::WebContents> contents2_;
  std::unique_ptr<content::WebContents> contents3_;

  std::vector<raw_ptr<content::WebContents, VectorExperimental>>
      tab_for_scoring_;
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
  policy_->max_time_since_last_use_to_restore_ = base::Minutes(90);
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
  policy_->max_time_since_last_use_to_restore_ = base::Minutes(1);
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
  auto iter = policy_->tab_data_.insert(
      std::make_pair(contents1_.get(), std::make_unique<TabData>()));
  TabData* tab_data = iter.first->second.get();
  tab_data->used_in_bg = true;
  EXPECT_TRUE(policy_->ShouldLoad(contents1_.get()));
}

TEST_F(SessionRestorePolicyTest, NotificationPermissionSetUsedInBgBit) {
  CreatePolicy(true);
  WaitForFinalTabScores();

  auto iter = policy_->tab_data_.find(contents1_.get());
  EXPECT_TRUE(iter != policy_->tab_data_.end());
  EXPECT_FALSE(iter->second->UsedInBg());

  // Allow |contents1_| to display notifications, this should cause the
  // |used_in_bg| bit to change to true.
  GetBrowserContext()->SetPermissionControllerForTesting(
      std::make_unique<content::MockPermissionController>());

  // Adding/Removing the tab for scoring will cause the callback to be called a
  // few times, ignore this.
  EXPECT_CALL(mock_, NotifyTabScoreChanged(::testing::_, ::testing::_))
      .Times(::testing::AnyNumber());

  policy_->RemoveTabForScoring(contents1_.get());
  policy_->AddTabForScoring(contents1_.get());
  WaitForFinalTabScores();

  iter = policy_->tab_data_.find(contents1_.get());
  EXPECT_TRUE(iter != policy_->tab_data_.end());
  EXPECT_TRUE(iter->second->UsedInBg());
}

TEST_F(SessionRestorePolicyTest, MultipleAllTabsDoneCallbacks) {
  CreatePolicy(true);
  WaitForFinalTabScores();

  // Another "all tabs scored" notification should be sent after more tabs
  // are added to the policy engine.
  std::unique_ptr<content::WebContents> contents4 =
      CreateAndInitTestWebContents(GURL("https://d.com"),
                                   base::TimeTicks::Now());
  std::unique_ptr<content::WebContents> contents5 =
      CreateAndInitTestWebContents(GURL("https://e.com"),
                                   base::TimeTicks::Now());
  policy_->AddTabForScoring(contents4.get());
  policy_->AddTabForScoring(contents5.get());
  WaitForFinalTabScores();

  performance_manager::MarkWebContentsAsUnloadedInBackgroundInSiteDataDb(
      contents4.get());
  performance_manager::MarkWebContentsAsUnloadedInBackgroundInSiteDataDb(
      contents5.get());
}

TEST_F(SessionRestorePolicyTest, CalculateAgeScore) {
  using TabData = TestSessionRestorePolicy::TabData;
  constexpr int kMonthInSeconds = 60 * 60 * 24 * 31;

  // Generate a bunch of random tab ages.
  std::vector<std::unique_ptr<TabData>> tab_data;
  tab_data.reserve(1000);

  for (size_t i = 0; i < 1000; ++i)
    tab_data.push_back(std::make_unique<TabData>());

  // Generate some known edge cases.
  size_t i = 0;
  tab_data[i++]->last_active = base::Milliseconds(-1001);
  tab_data[i++]->last_active = base::Milliseconds(-1000);
  tab_data[i++]->last_active = base::Milliseconds(-999);
  tab_data[i++]->last_active = base::Milliseconds(-500);
  tab_data[i++]->last_active = base::Milliseconds(0);
  tab_data[i++]->last_active = base::Milliseconds(500);
  tab_data[i++]->last_active = base::Milliseconds(999);
  tab_data[i++]->last_active = base::Milliseconds(1000);
  tab_data[i++]->last_active = base::Milliseconds(1001);

  // Generate a logarithmic selection of ages to test the whole range.
  tab_data[i++]->last_active = base::Seconds(-1000000);
  tab_data[i++]->last_active = base::Seconds(-100000);
  tab_data[i++]->last_active = base::Seconds(-10000);
  tab_data[i++]->last_active = base::Seconds(-1000);
  tab_data[i++]->last_active = base::Seconds(-100);
  tab_data[i++]->last_active = base::Seconds(-10);
  tab_data[i++]->last_active = base::Seconds(10);
  tab_data[i++]->last_active = base::Seconds(100);
  tab_data[i++]->last_active = base::Seconds(1000);
  tab_data[i++]->last_active = base::Seconds(10000);
  tab_data[i++]->last_active = base::Seconds(100000);
  tab_data[i++]->last_active = base::Seconds(1000000);
  tab_data[i++]->last_active = base::Seconds(10000000);

  // Generate a bunch more random ages.
  for (; i < tab_data.size(); ++i) {
    tab_data[i]->last_active =
        base::Seconds(base::RandInt(-kMonthInSeconds, kMonthInSeconds));
  }

  // Calculate the tab scores.
  for (i = 0; i < tab_data.size(); ++i) {
    tab_data[i]->score =
        TestSessionRestorePolicy::CalculateAgeScore(tab_data[i].get());
  }

  // Sort tabs by increasing last active time.
  std::sort(tab_data.begin(), tab_data.end(),
            [](const std::unique_ptr<TabData>& td1,
               const std::unique_ptr<TabData>& td2) {
              return td1->last_active < td2->last_active;
            });

  // The scores should be in decreasing order (>= is necessary because some
  // last active times collapse to the same score).
  for (i = 1; i < tab_data.size(); ++i)
    ASSERT_GE(tab_data[i - 1]->score, tab_data[i]->score);
}

TEST_F(SessionRestorePolicyTest, ScoreTab) {
  using TabData = TestSessionRestorePolicy::TabData;

  TabData td_bg;
  td_bg.used_in_bg = true;
  td_bg.last_active = base::Days(30);
  EXPECT_TRUE(TestSessionRestorePolicy::ScoreTab(&td_bg));

  TabData td_normal_young;
  TabData td_normal_old;
  td_normal_young.last_active = base::Seconds(1);
  td_normal_old.last_active = base::Days(7);
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

#if !BUILDFLAG(IS_ANDROID)
TEST_F(SessionRestorePolicyTest, FeatureUsageSetUsedInBgBit) {
  CreatePolicy(true);
  WaitForFinalTabScores();

  auto iter = policy_->tab_data_.find(contents1_.get());
  EXPECT_TRUE(iter != policy_->tab_data_.end());
  EXPECT_FALSE(iter->second->UsedInBg());

  // Indicates that |contents1_| might update its title while in background,
  // this should set the |used_in_bg_| bit.

  base::RunLoop run_loop;
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<performance_manager::PageNode> page_node,
                        base::OnceClosure closure) {
                       EXPECT_TRUE(page_node);
                       auto* impl =
                           performance_manager::GetSiteDataImplForPageNode(
                               page_node.get());
                       EXPECT_TRUE(impl);
                       impl->NotifyUpdatesTitleInBackground();
                       std::move(closure).Run();
                     },
                     performance_manager::PerformanceManager::
                         GetPrimaryPageNodeForWebContents(contents1_.get()),
                     run_loop.QuitClosure()));
  run_loop.Run();

  // Adding/Removing the tab for scoring will cause the callback to be called a
  // few times, ignore this.
  EXPECT_CALL(mock_, NotifyTabScoreChanged(::testing::_, ::testing::_))
      .Times(::testing::AnyNumber());

  policy_->RemoveTabForScoring(contents1_.get());
  policy_->AddTabForScoring(contents1_.get());
  WaitForFinalTabScores();

  iter = policy_->tab_data_.find(contents1_.get());
  EXPECT_TRUE(iter != policy_->tab_data_.end());
  EXPECT_TRUE(iter->second->UsedInBg());
}

TEST_F(SessionRestorePolicyTest, UnknownUsageSetUsedInBgBit) {
  auto contents = CreateTestWebContents();
  auto* tester = content::WebContentsTester::For(contents.get());
  ResourceCoordinatorTabHelper::CreateForWebContents(contents.get());
  tester->NavigateAndCommit(GURL("https://d.com"));

  // Adding/Removing the tab for scoring will cause the callback to be called a
  // few times, ignore this.
  EXPECT_CALL(mock_, NotifyTabScoreChanged(::testing::_, ::testing::_))
      .Times(::testing::AnyNumber());
  AddExtraTabForScoring(contents.get());

  CreatePolicy(true);
  WaitForFinalTabScores();

  base::RunLoop run_loop;
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<performance_manager::PageNode> page_node,
             base::OnceClosure closure) {
            EXPECT_TRUE(page_node);
            auto* impl = performance_manager::GetSiteDataImplForPageNode(
                page_node.get());
            EXPECT_TRUE(impl);
            performance_manager::SiteFeatureUsage title_feature_usage =
                impl->UpdatesTitleInBackground();
            EXPECT_EQ(
                performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
                title_feature_usage);
            std::move(closure).Run();
          },
          performance_manager::PerformanceManager::
              GetPrimaryPageNodeForWebContents(contents.get()),
          run_loop.QuitClosure()));
  run_loop.Run();

  auto iter = policy_->tab_data_.find(contents.get());
  EXPECT_TRUE(iter != policy_->tab_data_.end());
  EXPECT_TRUE(iter->second->UsedInBg());
}
#endif

}  // namespace resource_coordinator
