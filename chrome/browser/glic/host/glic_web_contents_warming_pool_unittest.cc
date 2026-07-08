// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"

#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace glic {

class FakeWebUIContentsContainer : public WebUIContentsContainer {
 public:
  explicit FakeWebUIContentsContainer(content::WebContents* web_contents)
      : web_contents_(web_contents) {}
  ~FakeWebUIContentsContainer() override = default;

  void AttachToHost(Host* host) override {}
  void SetVisibility(content::Visibility visibility) override {}
  void OnActuatingChanged(bool actuating) override {}
  void OnTaskTabsVisibilityChanged(bool has_visible_tab) override {}
  content::WebContents* web_contents() const override { return web_contents_; }

 private:
  raw_ptr<content::WebContents> web_contents_;
};

class TestGlicWebContentsWarmingPool : public GlicWebContentsWarmingPool {
 public:
  using GlicWebContentsWarmingPool::EnsurePreloadDelayed;

  TestGlicWebContentsWarmingPool(Profile* profile,
                                 content::TestWebContentsFactory* factory)
      : GlicWebContentsWarmingPool(profile), factory_(factory) {}

  std::unique_ptr<WebUIContentsContainer> CreateContainer() override {
    return std::make_unique<FakeWebUIContentsContainer>(
        factory_->CreateWebContents(profile_));
  }

  content::WebContents* GetWarmedWebContents() {
    return GetWarmedContainerForTesting()
               ? GetWarmedContainerForTesting()->web_contents()
               : nullptr;
  }

 private:
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

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicWebContentsWarmingPoolTest, EnsurePreload) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.EnsurePreload(
      GlicWebContentsWarmingPool::ContainerCreationReason::kInitialColdWarming);
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

  std::unique_ptr<WebUIContentsContainer> container =
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
  warming_pool.EnsurePreload();
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  std::unique_ptr<WebUIContentsContainer> container =
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
  warming_pool.EnsurePreload();
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
  warming_pool.EnsurePreload();
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  // Let the container expire.
  task_environment_.FastForwardBy(
      features::kGlicWebContentsWarmingPoolExpiryDelay.Get());

  // With the feature enabled (default), it should have reloaded.
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  std::unique_ptr<WebUIContentsContainer> container =
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
  warming_pool.EnsurePreload();
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
  warming_pool.EnsurePreload();
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  // Crash the container.
  content::WebContentsTester::For(warming_pool.GetWarmedWebContents())
      ->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);

  EXPECT_TRUE(warming_pool.TakeContainer());
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.HitStatus",
                                      WarmingPoolStatus::kCrashed, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest, EnsurePreloadReplacesCrashedContainer) {
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.EnsurePreload();
  content::WebContents* contents = warming_pool.GetWarmedWebContents();

  // Crash the container.
  content::WebContentsTester::For(contents)->SetIsCrashed(
      base::TERMINATION_STATUS_PROCESS_CRASHED, 0);

  warming_pool.EnsurePreload();
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_NE(contents, warming_pool.GetWarmedWebContents());
  EXPECT_FALSE(warming_pool.GetWarmedWebContents()->IsCrashed());
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
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_TRUE(warming_pool.TakeContainer());

  // Call it again quickly.
  EXPECT_TRUE(warming_pool.TakeContainer());

  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  task_environment_.FastForwardBy(features::kGlicWebContentsWarmingDelay.Get() -
                                  base::Seconds(1));
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
}

TEST_F(GlicWebContentsWarmingPoolTest, Clear) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.EnsurePreload();
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.Clear(GlicWebContentsWarmingPool::ClearReason::kMemoryPressure);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.WarmedContainerFate", 4,
                                      1);
}

TEST_F(GlicWebContentsWarmingPoolTest, WarmedContainerFate_Used) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.EnsurePreload();

  std::unique_ptr<WebUIContentsContainer> container =
      warming_pool.TakeContainer();

  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.WarmedContainerFate", 0,
                                      1);
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
  warming_pool.EnsurePreload();

  // Let it expire.
  task_environment_.FastForwardBy(
      features::kGlicWebContentsWarmingPoolExpiryDelay.Get());

  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.WarmedContainerFate", 1,
                                      1);
}

TEST_F(GlicWebContentsWarmingPoolTest, WarmedContainerFate_Crashed) {
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.EnsurePreload();

  // Crash the container.
  content::WebContentsTester::For(warming_pool.GetWarmedWebContents())
      ->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);

  // Trigger a check that replaces it.
  warming_pool.EnsurePreload();

  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.WarmedContainerFate", 3,
                                      1);
}

TEST_F(GlicWebContentsWarmingPoolTest, ShutdownClearsContainer) {
  base::HistogramTester histogram_tester;
  {
    TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                                &web_contents_factory_);
    warming_pool.EnsurePreload();
    // warming_pool goes out of scope here and is destroyed.
  }

  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.WarmedContainerFate", 2,
                                      1);
}

TEST_F(GlicWebContentsWarmingPoolTest, OnMemoryPressureStateful) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(base::kStatefulMemoryPressure);
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.EnsurePreload();
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.WarmedContainerFate", 4,
                                      1);

  // Background preloading schedules should be ignored while disabled.
  warming_pool.EnsurePreloadDelayed(
      GlicWebContentsWarmingPool::ContainerCreationReason::kRefill);
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  // Enabling should start a delayed refill timer since the pool is empty.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_TRUE(warming_pool.GetDelayTimerForTesting().IsRunning());

  warming_pool.GetDelayTimerForTesting().FireNow();
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
}

TEST_F(GlicWebContentsWarmingPoolTest,
       TakeContainerUnderStatefulMemoryPressure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(base::kStatefulMemoryPressure);
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  // TakeContainer() should still create and return a container synchronously
  // so UI launch doesn't fail, but should NOT schedule a background refill.
  std::unique_ptr<WebUIContentsContainer> container =
      warming_pool.TakeContainer();
  EXPECT_NE(nullptr, container);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  EXPECT_FALSE(warming_pool.GetDelayTimerForTesting().IsRunning());
  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingPool.HitStatus",
      GlicWebContentsWarmingPool::WarmingPoolStatus::kMemoryPressure, 1);
}

TEST_F(GlicWebContentsWarmingPoolTest,
       MaybeStartInitialWarmingUnderStatefulMemoryPressure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(base::kStatefulMemoryPressure);
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  EXPECT_FALSE(warming_pool.MaybeStartInitialWarming());
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_TRUE(warming_pool.MaybeStartInitialWarming());
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
}

TEST_F(GlicWebContentsWarmingPoolTest,
       MaybeStartInitialWarmingUnderStatelessMemoryPressure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(base::kStatefulMemoryPressure);
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  EXPECT_FALSE(warming_pool.MaybeStartInitialWarming());
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
}

TEST_F(GlicWebContentsWarmingPoolTest, OnMemoryPressureStateless) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(base::kStatefulMemoryPressure);
  base::HistogramTester histogram_tester;
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.EnsurePreload();
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  histogram_tester.ExpectUniqueSample("Glic.WarmingPool.WarmedContainerFate", 4,
                                      1);

  // Calling OnMemoryPressure(NONE) in stateless mode does nothing.
  warming_pool.OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);

  // TakeContainer() should create a container and schedule a background refill
  // because the pool was not disabled.
  std::unique_ptr<WebUIContentsContainer> container =
      warming_pool.TakeContainer();
  EXPECT_NE(nullptr, container);
  EXPECT_TRUE(warming_pool.GetDelayTimerForTesting().IsRunning());
  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingPool.HitStatus",
      GlicWebContentsWarmingPool::WarmingPoolStatus::kCold, 1);
}

}  // namespace glic
