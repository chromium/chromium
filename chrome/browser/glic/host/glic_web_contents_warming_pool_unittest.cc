// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class FakeWebUIContentsContainer : public WebUIContentsContainer {
 public:
  explicit FakeWebUIContentsContainer(content::WebContents* web_contents)
      : web_contents_(web_contents) {}
  ~FakeWebUIContentsContainer() override = default;

  void AttachToHost(Host* host) override {}
  content::WebContents* web_contents() const override { return web_contents_; }

 private:
  raw_ptr<content::WebContents> web_contents_;
};

class TestGlicWebContentsWarmingPool : public GlicWebContentsWarmingPool {
 public:
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
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicWebContentsWarming);
  }

 protected:
  using WarmingPoolStatus = GlicWebContentsWarmingPool::WarmingPoolStatus;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicWebContentsWarmingPoolTest, EnsurePreload) {
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.EnsurePreload();
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
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
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  EXPECT_TRUE(warming_pool.TakeContainer());

  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  task_environment_.FastForwardBy(features::kGlicWebContentsWarmingDelay.Get() -
                                  base::Seconds(1));
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());
}

TEST_F(GlicWebContentsWarmingPoolTest, TakeContainerRecordsExpiredStatus) {
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
  TestGlicWebContentsWarmingPool warming_pool(&profile_,
                                              &web_contents_factory_);
  warming_pool.EnsurePreload();
  EXPECT_TRUE(warming_pool.HasWarmedContainerForTesting());

  warming_pool.Clear();
  EXPECT_FALSE(warming_pool.HasWarmedContainerForTesting());
}

}  // namespace glic
