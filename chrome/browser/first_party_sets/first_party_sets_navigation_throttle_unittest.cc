// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"

namespace {

const char kExampleURL[] = "https://example.com";

}  // namespace

namespace first_party_sets {

class FirstPartySetsNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  FirstPartySetsNavigationThrottleTest() {
    features_.InitAndEnableFeatureWithParameters(
        features::kFirstPartySets,
        {{features::kFirstPartySetsClearSiteDataOnChangedSets.name, "true"}});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    subframe_ = content::RenderFrameHostTester::For(main_rfh())
                    ->AppendChild("subframe");
  }

  content::RenderFrameHost* subframe() { return subframe_; }

 private:
  base::test::ScopedFeatureList features_;
  raw_ptr<content::RenderFrameHost> subframe_;
};

TEST_F(FirstPartySetsNavigationThrottleTest,
       MaybeCreateNavigationThrottle_ClearingFeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFirstPartySets,
      {{features::kFirstPartySetsClearSiteDataOnChangedSets.name, "false"}});

  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());

  EXPECT_FALSE(
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle));
}

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

TEST_F(FirstPartySetsNavigationThrottleTest, WillStartRequest_Defer) {
  // Create throttle for main frames.
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  ASSERT_TRUE(handle.IsInOutermostMainFrame());
  auto throttle =
      FirstPartySetsNavigationThrottle::MaybeCreateNavigationThrottle(&handle);
  EXPECT_TRUE(throttle);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillStartRequest().action());
}

}  // namespace first_party_sets
