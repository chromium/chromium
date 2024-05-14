// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"

using ::testing::_;
using ::testing::Return;

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

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockDataSharingService mock_data_sharing_service_;
};

// Tests if a web page should be intercepted.
TEST_F(DataSharingNavigationThrottleUnitTest, TestCheckIfShouldIntercept) {
  content::MockNavigationHandle test_handle(
      GURL("https://www.example.com/"), web_contents()->GetPrimaryMainFrame());
  auto throttle = std::make_unique<DataSharingNavigationThrottle>(&test_handle);
  throttle->SetServiceForTesting(&mock_data_sharing_service_);

  EXPECT_CALL(mock_data_sharing_service_,
              ShouldInterceptNavigationForShareURL(_))
      .WillOnce(Return(true));
  EXPECT_EQ(DataSharingNavigationThrottle::CANCEL,
            throttle->WillStartRequest());

  EXPECT_CALL(mock_data_sharing_service_,
              ShouldInterceptNavigationForShareURL(_))
      .WillOnce(Return(false));
  EXPECT_EQ(DataSharingNavigationThrottle::PROCEED,
            throttle->WillStartRequest());
}

}  // namespace data_sharing
