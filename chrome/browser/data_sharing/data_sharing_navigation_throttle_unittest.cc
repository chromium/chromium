// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"

namespace data_sharing {

class DataSharingNavigationThrottleUnitTest
    : public ChromeRenderViewHostTestHarness {
 public:
  DataSharingNavigationThrottleUnitTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{data_sharing::features::kDataSharingFeature, {}}}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests if a web page should be intercepted.
TEST_F(DataSharingNavigationThrottleUnitTest, TestCheckIfShouldIntercept) {
  content::MockNavigationHandle test_handle(
      GURL("https://www.example.com/"), web_contents()->GetPrimaryMainFrame());
  auto throttle = std::make_unique<DataSharingNavigationThrottle>(&test_handle);

  EXPECT_EQ(DataSharingNavigationThrottle::PROCEED,
            throttle->WillStartRequest());
}

}  // namespace data_sharing
