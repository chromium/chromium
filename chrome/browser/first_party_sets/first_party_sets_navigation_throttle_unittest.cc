// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_navigation_throttle.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/base/features.h"
#include "third_party/blink/public/common/features.h"

namespace {

const char kExampleURL[] = "https://example.com";

}  // namespace

namespace first_party_sets {

class FirstPartySetsNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  FirstPartySetsNavigationThrottleTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    features_.InitWithFeaturesAndParameters(
        {
            {net::features::kWaitForFirstPartySetsInit,
             {{net::features::
                   kWaitForFirstPartySetsInitNavigationThrottleTimeout.name,
               "2s"}}},
        },
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    subframe_ = content::RenderFrameHostTester::For(main_rfh())
                    ->AppendChild("subframe");

    service_ =
        FirstPartySetsPolicyServiceFactory::GetForBrowserContext(profile());
    ASSERT_NE(service_, nullptr);

    // We can't avoid eagerly initializing the service, due to
    // indirection/caching in the factory infrastructure. So we wait for the
    // initialization to complete, and then reset the instance so that we can
    // call InitForTesting.
    base::RunLoop run_loop;
    service_->WaitForFirstInitCompleteForTesting(run_loop.QuitClosure());
    run_loop.Run();
    service_->ResetForTesting();
  }

  content::RenderFrameHost* subframe() { return subframe_; }

  FirstPartySetsPolicyService* service() { return service_; }

 private:
  base::test::ScopedFeatureList features_;
  raw_ptr<content::RenderFrameHost, DanglingUntriaged> subframe_;
  ScopedMockFirstPartySetsHandler first_party_sets_handler_;
  raw_ptr<FirstPartySetsPolicyService, DanglingUntriaged> service_;
};

TEST_F(FirstPartySetsNavigationThrottleTest,
       MaybeCreateNavigationThrottle_ClearingFeatureEnabled) {
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());

  EXPECT_TRUE(
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle));
}

TEST_F(FirstPartySetsNavigationThrottleTest,
       MaybeCreateNavigationThrottle_OnlyCreateForOuterMostframes) {
  // Create throttle for main frames.
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());
  EXPECT_TRUE(
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle));

  // Never create throttle for subframes.
  handle.set_render_frame_host(subframe());
  ASSERT_FALSE(handle.IsInOutermostMainFrame());
  EXPECT_FALSE(
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle));
}

TEST_F(FirstPartySetsNavigationThrottleTest,
       MaybeCreateNavigationThrottle_IrregularProfile) {
  // Create throttle for main frames.
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());
  profile()->SetGuestSession(true);
  EXPECT_FALSE(
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle));
}

TEST_F(FirstPartySetsNavigationThrottleTest,
       MaybeCreateNavigationThrottle_ServiceNotReady) {
  // Create throttle for main frames.
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());
  EXPECT_TRUE(
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle));
}

TEST_F(FirstPartySetsNavigationThrottleTest,
       MaybeCreateNavigationThrottle_ServiceReady) {
  // Create throttle for main frames.
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());

  // Never create if service is ready.
  service()->InitForTesting();
  EXPECT_FALSE(
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle));
}

TEST_F(FirstPartySetsNavigationThrottleTest, WillStartRequest_Defer) {
  // Create throttle for main frames.
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());
  auto throttle =
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle);
  EXPECT_TRUE(throttle);
  ASSERT_FALSE(
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(profile())
          ->is_ready());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillStartRequest().action());
}

TEST_F(FirstPartySetsNavigationThrottleTest, WillStartRequest_Proceed) {
  base::HistogramTester histograms;

  // Create throttle for main frames.
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());
  auto throttle =
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle);
  EXPECT_TRUE(throttle);

  // Service is ready after the throttle is created.
  service()->InitForTesting();
  ASSERT_TRUE(
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(profile())
          ->is_ready());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());

  histograms.ExpectTotalCount("FirstPartySets.NavigationThrottle.ResumeDelta",
                              0);
}

TEST_F(FirstPartySetsNavigationThrottleTest, ResumeOnReady) {
  base::HistogramTester histograms;

  // Create throttle for main frames.
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());
  auto throttle =
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle);
  EXPECT_TRUE(throttle);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillStartRequest().action());
  EXPECT_TRUE(throttle->GetTimerForTesting().IsRunning());

  // Verify that the throttle will be resumed once ready.
  base::RunLoop run_loop;
  throttle->set_resume_callback_for_testing(run_loop.QuitClosure());
  service()->InitForTesting();

  run_loop.Run();

  histograms.ExpectTotalCount("FirstPartySets.NavigationThrottle.ResumeDelta",
                              1);

  EXPECT_FALSE(throttle->GetTimerForTesting().IsRunning());
  histograms.ExpectUniqueSample(
      "FirstPartySets.NavigationThrottle.ResumeOnTimeout", false,
      /*expected_bucket_count=*/1);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

TEST_F(FirstPartySetsNavigationThrottleTest, ResumeOnTimeout) {
  base::HistogramTester histograms;

  // Create throttle for main frames.
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());
  auto throttle =
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle);
  EXPECT_TRUE(throttle);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillStartRequest().action());

  throttle->set_resume_callback_for_testing(base::DoNothing());
  // Verify that the throttle will be resumed due to timeout.
  task_environment()->FastForwardBy(base::Seconds(2));
  histograms.ExpectUniqueSample(
      "FirstPartySets.NavigationThrottle.ResumeOnTimeout", true,
      /*expected_bucket_count=*/1);

  // Verify that resume on service ready is no-op.
  service()->InitForTesting();
  histograms.ExpectBucketCount(
      "FirstPartySets.NavigationThrottle.ResumeOnTimeout", false,
      /*expected_count=*/0);

  histograms.ExpectTotalCount("FirstPartySets.NavigationThrottle.ResumeDelta",
                              1);
}

class FirstPartySetsNavigationThrottleNoDelayTest
    : public FirstPartySetsNavigationThrottleTest {
 public:
  FirstPartySetsNavigationThrottleNoDelayTest() {
    features_.InitAndEnableFeatureWithParameters(
        net::features::kWaitForFirstPartySetsInit,
        {
            {net::features::kWaitForFirstPartySetsInitNavigationThrottleTimeout
                 .name,
             "0s"},
        });
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(FirstPartySetsNavigationThrottleNoDelayTest,
       MaybeCreateNavigationThrottle) {
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());
  EXPECT_FALSE(
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle));
}

}  // namespace first_party_sets
