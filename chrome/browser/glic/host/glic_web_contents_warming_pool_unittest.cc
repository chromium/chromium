// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"

#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/glic_web_client_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/scenario_api/performance_scenario_test_support.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace glic {

class FakeWebContentsManager : public GlicWebContentsManager {
 public:
  explicit FakeWebContentsManager(content::WebContents* web_contents)
      : web_contents_(web_contents) {}
  ~FakeWebContentsManager() override = default;

  void AttachToHost(Host* host) override {}
  void SetVisibility(content::Visibility visibility) override {}
  void OnActuatingChanged(bool actuating) override {}
  void OnTaskTabsVisibilityChanged(bool has_visible_tab) override {}
  content::WebContents* active_web_contents() const override {
    return web_contents_;
  }
  content::WebContents* guest_contents() const override {
    return web_client_manager_.web_client_contents();
  }
  base::CallbackListSubscription RegisterWebContentsChangedCallback(
      WebContentsChangedCallback callback) override {
    return base::CallbackListSubscription();
  }
  GlicWebClientManager& web_client_manager() override {
    return web_client_manager_;
  }
  bool ShouldReloadOnShow() const override {
    return should_reload_on_show_ ||
           (web_contents_ ? web_contents_->IsCrashed() : false);
  }
  void set_should_reload_on_show(bool reload) {
    should_reload_on_show_ = reload;
  }

 private:
  GlicWebClientManager web_client_manager_;
  raw_ptr<content::WebContents> web_contents_;
  bool should_reload_on_show_ = false;
};

class TestGlicWebContentsWarmingPool : public GlicWebContentsWarmingPool {
 public:
  TestGlicWebContentsWarmingPool(Profile* profile,
                                 content::TestWebContentsFactory* factory)
      : GlicWebContentsWarmingPool(profile), factory_(factory) {}

 private:
  std::unique_ptr<GlicWebContentsManager> CreateContainer() override {
    return std::make_unique<FakeWebContentsManager>(
        factory_->CreateWebContents(profile()));
  }

  raw_ptr<content::TestWebContentsFactory> factory_;
};

class GlicWebContentsWarmingPoolTest : public testing::Test {
 public:
  GlicWebContentsWarmingPoolTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  using WarmingPoolStatus = GlicWebContentsWarmingPool::WarmingPoolStatus;
  using ReloadAfterExpiryStatus =
      GlicWebContentsWarmingPool::ReloadAfterExpiryStatus;
  using WarmedContainerFate = GlicWebContentsWarmingPool::WarmedContainerFate;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicWebContentsWarmingPoolTest, MaybeStartWarming) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingPool.ContainerCreationReason",
      GlicWebContentsWarmingPool::ContainerCreationReason::kInitialColdWarming,
      1);
}

TEST_F(GlicWebContentsWarmingPoolTest, TakeContainerCreatesContainer) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  std::unique_ptr<GlicWebContentsManager> container =
      warming_pool.TakeContainer();
  EXPECT_TRUE(container);
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.HitStatus",
                                      WarmingPoolStatus::kCold, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingPool.ContainerCreationReason",
      GlicWebContentsWarmingPool::ContainerCreationReason::
          kUserTriggeredColdStart,
      1);
}

TEST_F(GlicWebContentsWarmingPoolTest, TakeContainerUsesPreloadedContainer) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  std::unique_ptr<GlicWebContentsManager> container =
      warming_pool.TakeContainer();
  EXPECT_TRUE(container);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.HitStatus",
                                      WarmingPoolStatus::kHit, 1);
  histogram_tester.ExpectTotalCount("Glic.WarmingPool.TimeSinceCreatedAtHit",
                                    1);
}

TEST_F(GlicWebContentsWarmingPoolTest, TakeContainerTriggersDelayedWarming) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_TRUE(warming_pool.TakeContainer());

  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  task_environment_.FastForwardBy(features::kGlicWebContentsWarmingDelay.Get() -
                                  base::Seconds(1));
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.ContainerCreationReason",
      GlicWebContentsWarmingPool::ContainerCreationReason::kRefill, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, TakeContainerRecordsExpiredStatus) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(kGlicReloadWebContentsAfterExpiry);

  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  // Let the container expire.
  task_environment_.FastForwardBy(
      features::kGlicWebContentsWarmingPoolExpiryDelay.Get());
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  EXPECT_TRUE(warming_pool.TakeContainer());
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.HitStatus",
                                      WarmingPoolStatus::kExpired, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingPool.ReloadAfterExpiry",
      ReloadAfterExpiryStatus::kNotReloadedFeatureDisabled, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, TakeContainerReloadsAfterExpiry) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  // Let the container expire.
  task_environment_.FastForwardBy(
      features::kGlicWebContentsWarmingPoolExpiryDelay.Get());

  // With the feature enabled (default), it should have reloaded.
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  std::unique_ptr<GlicWebContentsManager> container =
      warming_pool.TakeContainer();
  EXPECT_TRUE(container);

  // HitStatus should be kHit because it was reloaded, not Cold or Expired.
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.HitStatus",
                                      WarmingPoolStatus::kHit, 1);
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.ReloadAfterExpiry",
                                      ReloadAfterExpiryStatus::kReloaded, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, TakeContainerLimitsReloadCount) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  // Default limit is 4. Fast forward 4 times to use up all reloads.
  for (int i = 0; i < 4; ++i) {
    task_environment_.FastForwardBy(
        features::kGlicWebContentsWarmingPoolExpiryDelay.Get());
    EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  }

  // 5th expiry should exceed the limit.
  task_environment_.FastForwardBy(
      features::kGlicWebContentsWarmingPoolExpiryDelay.Get());
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  histogram_tester.ExpectBucketCount("Glic.WarmingPool.ReloadAfterExpiry",
                                     ReloadAfterExpiryStatus::kReloaded, 4);
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.ReloadAfterExpiry",
      ReloadAfterExpiryStatus::kNotReloadedLimitReached, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.ContainerCreationReason",
      GlicWebContentsWarmingPool::ContainerCreationReason::kReloadAfterExpiry,
      4);
}

TEST_F(GlicWebContentsWarmingPoolTest, TakeContainerReplacesCrashedContainer) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  auto warmed_contents = warming_pool.GetWarmedWebContents();
  ASSERT_TRUE(warmed_contents);
  content::WebContents* contents = warmed_contents->webui_contents;
  ASSERT_TRUE(contents);

  // Crash the container.
  content::WebContentsTester::For(contents)->SetIsCrashed(
      base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
  ASSERT_TRUE(contents->IsCrashed());

  std::unique_ptr<GlicWebContentsManager> taken = warming_pool.TakeContainer();
  EXPECT_TRUE(taken);
  EXPECT_NE(contents, taken->active_web_contents());
  EXPECT_FALSE(taken->active_web_contents()->IsCrashed());
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.HitStatus",
                                      WarmingPoolStatus::kCrashed, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, TakeContainerReplacesErroredContainer) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  auto* container = static_cast<FakeWebContentsManager*>(
      warming_pool.GetWarmedContainerForTesting());
  ASSERT_TRUE(container);
  content::WebContents* contents = container->active_web_contents();
  ASSERT_TRUE(contents);

  // Set the container to an error state.
  container->set_should_reload_on_show(true);
  ASSERT_TRUE(container->ShouldReloadOnShow());

  std::unique_ptr<GlicWebContentsManager> taken = warming_pool.TakeContainer();
  EXPECT_TRUE(taken);
  EXPECT_NE(contents, taken->active_web_contents());
  EXPECT_FALSE(taken->ShouldReloadOnShow());
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.HitStatus",
                                      WarmingPoolStatus::kCrashed, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, WarmingDelayTooLongAndNotScheduled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicWebContentsWarming,
      {{"glic-web-contents-warming-delay", "8d"}});

  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_TRUE(warming_pool.TakeContainer());
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());
}

TEST_F(GlicWebContentsWarmingPoolTest, TakeContainerBeforeWarmingComplete) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_TRUE(warming_pool.TakeContainer());
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.HitStatus",
                                      WarmingPoolStatus::kCold, 1);

  // Call it again quickly while the backfill delay timer is running.
  EXPECT_TRUE(warming_pool.TakeContainer());
  histogram_tester.ExpectBucketCount("Glic.WarmingPool.HitStatus",
                                     WarmingPoolStatus::kCold, 1);
  histogram_tester.ExpectBucketCount("Glic.WarmingPool.HitStatus",
                                     WarmingPoolStatus::kPendingBackfill, 1);

  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  task_environment_.FastForwardBy(features::kGlicWebContentsWarmingDelay.Get() -
                                  base::Seconds(1));
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
}

TEST_F(GlicWebContentsWarmingPoolTest,
       TakeContainerRecordsPendingBackfillAfterMemoryPressureRelieved) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  // Trigger critical memory pressure to clear the container.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  // Relieve memory pressure. Since the pool was active, it schedules a delayed
  // backfill.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_TRUE(warming_pool.GetDelayTimerForTesting().IsRunning());

  // Taking container while the backfill timer is running records
  // kPendingBackfill.
  EXPECT_TRUE(warming_pool.TakeContainer());
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.HitStatus",
                                      WarmingPoolStatus::kPendingBackfill, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, Shutdown) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.Shutdown();
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingPool.WarmedContainerFate",
      WarmedContainerFate::kDeletedOnChromeClosed, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, WarmedContainerFate_Used) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));

  std::unique_ptr<GlicWebContentsManager> container =
      warming_pool.TakeContainer();

  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.WarmedContainerFate",
                                      WarmedContainerFate::kUsed, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, WarmedContainerFate_Expired) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(kGlicReloadWebContentsAfterExpiry);

  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));

  // Let it expire.
  task_environment_.FastForwardBy(
      features::kGlicWebContentsWarmingPoolExpiryDelay.Get());

  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.WarmedContainerFate",
                                      WarmedContainerFate::kExpired, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, WarmedContainerFate_Crashed) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  auto warmed_contents = warming_pool.GetWarmedWebContents();
  ASSERT_TRUE(warmed_contents);
  content::WebContents* contents = warmed_contents->webui_contents;
  ASSERT_TRUE(contents);

  // Crash the container.
  content::WebContentsTester::For(contents)->SetIsCrashed(
      base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
  ASSERT_TRUE(contents->IsCrashed());

  // Trigger a check that replaces it.
  warming_pool.TakeContainer();

  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.WarmedContainerFate",
                                      WarmedContainerFate::kCrashed, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, ShutdownClearsContainer) {
  base::HistogramTester histogram_tester;
  {
    TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                                &web_contents_factory_);
    ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
    // warming_pool goes out of scope here and is destroyed.
  }

  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingPool.WarmedContainerFate",
      WarmedContainerFate::kDeletedOnChromeClosed, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest,
       OnMemoryPressureDoesNotRefillWithoutCritical) {
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  // Receiving non-critical memory pressure without previously being under
  // critical pressure should NOT trigger a delayed refill.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());

  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());
}

TEST_F(GlicWebContentsWarmingPoolTest,
       OnMemoryPressureDoesNotRefillIfInitialWarmingNeverAttempted) {
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  // Receiving critical memory pressure at startup without ever having called
  // initial warming should not trigger a delayed refill when pressure subsides.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());

  // Attempting initial warming while under critical pressure records the
  // attempt even though container creation is blocked.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  // Now when memory pressure drops, the delayed refill timer should start.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_TRUE(warming_pool.GetDelayTimerForTesting().IsRunning());
}

TEST_F(GlicWebContentsWarmingPoolTest,
       OnMemoryPressureDoesNotRefillIfShutDownPriorToMemoryPressure) {
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  // Explicitly shut down the container prior to any memory pressure.
  warming_pool.Shutdown();
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  // Receiving critical memory pressure when already shut down should not
  // schedule a refill when memory pressure subsides.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());
}

TEST_F(GlicWebContentsWarmingPoolTest,
       TakeContainerUnderCriticalMemoryPressure) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingPool.WarmedContainerFate",
      WarmedContainerFate::kDeletedOnMemoryPressure, 1);

  // TakeContainer() should still create and return a container synchronously
  // so UI launch doesn't fail, but should NOT schedule a background refill.
  ASSERT_TRUE(warming_pool.TakeContainer());
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());
  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingPool.HitStatus",
      GlicWebContentsWarmingPool::WarmingPoolStatus::kMemoryPressure, 1);

  // When memory pressure subsides, the delayed refill should start because the
  // pool became active.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_TRUE(warming_pool.GetDelayTimerForTesting().IsRunning());
}

TEST_F(GlicWebContentsWarmingPoolTest,
       ExpiryTimerStoppedUnderCriticalMemoryPressure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGlicReloadWebContentsAfterExpiry);
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);

  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.IsExpiryTimerRunningForTesting());

  // When critical memory pressure is applied, the container is destroyed and
  // the expiry timer should be stopped.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_FALSE(warming_pool.IsExpiryTimerRunningForTesting());

  // Once memory pressure is relieved, the delay timer will run, after which a
  // new container will be created and the expiry timer should be running again.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_TRUE(warming_pool.GetDelayTimerForTesting().IsRunning());
  task_environment_.FastForwardBy(
      base::Milliseconds(features::kGlicWarmingDelayMs.Get()));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.IsExpiryTimerRunningForTesting());
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.ContainerCreationReason",
      GlicWebContentsWarmingPool::ContainerCreationReason::
          kMemoryPressureRecovery,
      1);
}

TEST_F(GlicWebContentsWarmingPoolTest,
       MemoryPressureRecoversAfterExpiryReload) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGlicReloadWebContentsAfterExpiry);
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);

  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  // Fast-forward to trigger OnContainerExpired() and reload once.
  task_environment_.FastForwardBy(
      features::kGlicWebContentsWarmingPoolExpiryDelay.Get());
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.IsExpiryTimerRunningForTesting());

  // Under critical memory pressure, the reloaded container is cleared.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());

  // When memory pressure subsides, delayed refill must start because the pool
  // remained active through the reload.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_TRUE(warming_pool.GetDelayTimerForTesting().IsRunning());
}

TEST_F(GlicWebContentsWarmingPoolTest,
       MaybeStartWarmingUnderCriticalMemoryPressure) {
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);

  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  EXPECT_FALSE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
}

TEST_F(GlicWebContentsWarmingPoolTest,
       TakeContainerWhenInitialWarmingBlockedByMemoryPressure) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  ASSERT_TRUE(warming_pool.TakeContainer());
  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingPool.HitStatus",
      GlicWebContentsWarmingPool::WarmingPoolStatus::kMemoryPressure, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest,
       TakeContainerWhenRefillCancelledByMemoryPressure) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  // First take creates a container and starts the delayed refill timer.
  ASSERT_TRUE(warming_pool.TakeContainer());
  EXPECT_TRUE(warming_pool.GetDelayTimerForTesting().IsRunning());
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.HitStatus",
      GlicWebContentsWarmingPool::WarmingPoolStatus::kCold, 1);

  // Critical memory pressure arrives, cancelling the refill timer.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());

  // Second take occurs: the refill was cancelled by memory pressure,
  // so HitStatus should record kMemoryPressure.
  ASSERT_TRUE(warming_pool.TakeContainer());
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.HitStatus",
      GlicWebContentsWarmingPool::WarmingPoolStatus::kMemoryPressure, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest,
       ExpiryTimerRemainsStoppableAfterReloadAfterExpiry) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.IsExpiryTimerRunningForTesting());

  // Fast-forward to trigger OnContainerExpired().
  // OnContainerExpired() reloads the container and restarts expiry_timer_.
  task_environment_.FastForwardBy(
      features::kGlicWebContentsWarmingPoolExpiryDelay.Get());

  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.IsExpiryTimerRunningForTesting());

  // Taking the reloaded container must stop the expiry timer so it cannot fire
  // when warmed_container_ is null.
  warming_pool.TakeContainer();
  EXPECT_FALSE(warming_pool.IsExpiryTimerRunningForTesting());
}

TEST_F(GlicWebContentsWarmingPoolTest, ProfileDestructionClearsWarmingPool) {
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.IsExpiryTimerRunningForTesting());

  // Simulate profile destruction.
  profile_.MaybeSendDestroyedNotification();

  // The warming pool must be cleared immediately upon profile destruction.
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_FALSE(warming_pool.IsExpiryTimerRunningForTesting());
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());

  // Further attempts to warm after shutdown must be rejected.
  EXPECT_FALSE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
}

TEST_F(GlicWebContentsWarmingPoolTest,
       BackfillWarmingWithPerformanceManagerTriggersWhenIdle) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicWebContentsWarming,
                            features::
                                kGlicBackfillWarmingUsePerformanceManager},
      /*disabled_features=*/{});

  auto test_helper =
      performance_scenarios::PerformanceScenarioTestHelper::Create();
  ASSERT_TRUE(test_helper);
  test_helper->SetLoadingScenario(
      performance_scenarios::ScenarioScope::kGlobal,
      performance_scenarios::LoadingScenario::kFocusedPageLoading);
  test_helper->SetInputScenario(performance_scenarios::ScenarioScope::kGlobal,
                                performance_scenarios::InputScenario::kNoInput);

  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  // Taking the container drains the pool and schedules backfill via PM.
  auto taken = warming_pool.TakeContainer();
  EXPECT_TRUE(taken);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.GetBackfillSchedulerForTesting().IsScheduled());

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.GetBackfillSchedulerForTesting().IsScheduled());

  // Fast-forwarding time past the default warming delay does NOT trigger
  // backfill because Performance Manager observation is trusted without a
  // racing timer.
  task_environment_.FastForwardBy(features::kGlicWebContentsWarmingDelay.Get() +
                                  base::Seconds(30));
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.GetBackfillSchedulerForTesting().IsScheduled());

  // Transition global loading scenario to idle (kNoPageLoading).
  test_helper->SetLoadingScenario(
      performance_scenarios::ScenarioScope::kGlobal,
      performance_scenarios::LoadingScenario::kNoPageLoading);
  task_environment_.RunUntilIdle();

  // Container is backfilled!
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_FALSE(warming_pool.GetBackfillSchedulerForTesting().IsScheduled());
}

TEST_F(
    GlicWebContentsWarmingPoolTest,
    BackfillWarmingWithPerformanceManagerFallbackWhenObserverListUnavailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicWebContentsWarming,
                            features::
                                kGlicBackfillWarmingUsePerformanceManager},
      /*disabled_features=*/{});

  // Note: No PerformanceScenarioTestHelper is created, so observer list is
  // nullptr.
  ASSERT_EQ(performance_scenarios::PerformanceScenarioObserverList::GetForScope(
                performance_scenarios::ScenarioScope::kGlobal),
            nullptr);

  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  ASSERT_TRUE(warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  auto taken = warming_pool.TakeContainer();
  EXPECT_TRUE(taken);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.GetBackfillSchedulerForTesting().IsScheduled());

  // Advance time up to just before fallback timeout
  // (features::kGlicWebContentsWarmingDelay).
  task_environment_.FastForwardBy(features::kGlicWebContentsWarmingDelay.Get() -
                                  base::Seconds(1));
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_TRUE(warming_pool.GetBackfillSchedulerForTesting().IsScheduled());

  // Fallback timeout expires, forcing backfill.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_FALSE(warming_pool.GetBackfillSchedulerForTesting().IsScheduled());
}

}  // namespace glic
