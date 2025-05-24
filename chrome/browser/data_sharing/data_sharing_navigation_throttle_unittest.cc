// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/data_sharing/data_sharing_navigation_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/collaboration/test_support/mock_collaboration_service.h"
#include "components/data_sharing/public/data_sharing_utils.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"

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
    test_handle_ = std::make_unique<content::MockNavigationHandle>(
        GURL("https://www.example.com/"),
        web_contents()->GetPrimaryMainFrame());
    test_registry_ = std::make_unique<content::MockNavigationThrottleRegistry>(
        test_handle_.get(),
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    throttle_ =
        std::make_unique<DataSharingNavigationThrottle>(*test_registry_);
    throttle_->SetServiceForTesting(&mock_collaboration_service_);
    DataSharingNavigationUtils::GetInstance()->set_clock_for_testing(&clock_);
  }

  void TearDown() override {
    DataSharingUtils::SetShouldInterceptForTesting(std::nullopt);
    DataSharingNavigationUtils::GetInstance()->set_clock_for_testing(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  collaboration::MockCollaborationService mock_collaboration_service_;
  std::unique_ptr<content::MockNavigationHandle> test_handle_;
  std::unique_ptr<content::MockNavigationThrottleRegistry> test_registry_;
  std::unique_ptr<DataSharingNavigationThrottle> throttle_;
  base::SimpleTestClock clock_;
};

// Tests if a web page should be intercepted.
TEST_F(DataSharingNavigationThrottleUnitTest, TestCheckIfShouldIntercept) {
  DataSharingUtils::SetShouldInterceptForTesting(true);
  EXPECT_CALL(*test_handle_, HasUserGesture()).WillRepeatedly(Return(false));
  EXPECT_CALL(mock_collaboration_service_,
              HandleShareURLNavigationIntercepted(_, _, _))
      .Times(0);
  EXPECT_EQ(DataSharingNavigationThrottle::CANCEL,
            throttle_->WillStartRequest());

  DataSharingUtils::SetShouldInterceptForTesting(false);
  EXPECT_EQ(DataSharingNavigationThrottle::PROCEED,
            throttle_->WillStartRequest());
}

TEST_F(DataSharingNavigationThrottleUnitTest,
       TestRendererInitiatedNavigationWithUserGesture) {
  EXPECT_CALL(*test_handle_, HasUserGesture()).WillOnce(Return(true));

  DataSharingUtils::SetShouldInterceptForTesting(true);
  EXPECT_CALL(mock_collaboration_service_,
              HandleShareURLNavigationIntercepted(_, _, _))
      .Times(1);
  EXPECT_EQ(DataSharingNavigationThrottle::CANCEL,
            throttle_->WillStartRequest());
}

TEST_F(DataSharingNavigationThrottleUnitTest, TestBrowserInitiatedNavigation) {
  test_handle_->set_is_renderer_initiated(false);

  DataSharingUtils::SetShouldInterceptForTesting(true);
  EXPECT_CALL(mock_collaboration_service_,
              HandleShareURLNavigationIntercepted(_, _, _))
      .Times(1);
  EXPECT_EQ(DataSharingNavigationThrottle::CANCEL,
            throttle_->WillStartRequest());
}

TEST_F(DataSharingNavigationThrottleUnitTest,
       TestRendererInitiatedNavigationWithRecentUserGesture) {
  // Create the first throttle with user gesture, but don't intercept it.
  EXPECT_CALL(*test_handle_, HasUserGesture()).WillRepeatedly(Return(true));

  DataSharingUtils::SetShouldInterceptForTesting(false);
  EXPECT_CALL(mock_collaboration_service_,
              HandleShareURLNavigationIntercepted(_, _, _))
      .Times(0);
  EXPECT_EQ(DataSharingNavigationThrottle::PROCEED,
            throttle_->WillStartRequest());

  // Create a new throttle, this time without user gesture and interception.
  EXPECT_CALL(*test_handle_, HasUserGesture()).WillRepeatedly(Return(false));
  throttle_ =
      std::make_unique<DataSharingNavigationThrottle>(*test_registry_);
  throttle_->SetServiceForTesting(&mock_collaboration_service_);
  DataSharingUtils::SetShouldInterceptForTesting(true);
  EXPECT_CALL(mock_collaboration_service_,
              HandleShareURLNavigationIntercepted(_, _, _))
      .Times(0);
  EXPECT_EQ(DataSharingNavigationThrottle::CANCEL,
            throttle_->WillStartRequest());
}

TEST_F(DataSharingNavigationThrottleUnitTest,
       TestRendererInitiatedRedirectWithRecentUserGesture) {
  // Create the first throttle with user gesture, but don't intercept it.
  EXPECT_CALL(*test_handle_, HasUserGesture()).WillRepeatedly(Return(true));

  DataSharingUtils::SetShouldInterceptForTesting(false);
  EXPECT_CALL(mock_collaboration_service_,
              HandleShareURLNavigationIntercepted(_, _, _))
      .Times(0);
  EXPECT_EQ(DataSharingNavigationThrottle::PROCEED,
            throttle_->WillStartRequest());
  clock_.Advance(base::Milliseconds(100));

  // Create a new throttle, this time without user gesture and interception.
  test_handle_->set_redirect_chain(
      std::vector<GURL>(3, GURL("http://foo.com")));
  EXPECT_CALL(*test_handle_, HasUserGesture()).WillRepeatedly(Return(false));
  throttle_ =
      std::make_unique<DataSharingNavigationThrottle>(*test_registry_);
  throttle_->SetServiceForTesting(&mock_collaboration_service_);
  DataSharingUtils::SetShouldInterceptForTesting(true);
  EXPECT_CALL(mock_collaboration_service_,
              HandleShareURLNavigationIntercepted(_, _, _))
      .Times(1);
  EXPECT_EQ(DataSharingNavigationThrottle::CANCEL,
            throttle_->WillStartRequest());
}

TEST_F(DataSharingNavigationThrottleUnitTest,
       TestRendererInitiatedRedirectWithRecentUserGestureExpired) {
  // Create the first throttle with user gesture, but don't intercept it.
  EXPECT_CALL(*test_handle_, HasUserGesture()).WillRepeatedly(Return(true));

  DataSharingUtils::SetShouldInterceptForTesting(false);
  EXPECT_CALL(mock_collaboration_service_,
              HandleShareURLNavigationIntercepted(_, _, _))
      .Times(0);
  EXPECT_EQ(DataSharingNavigationThrottle::PROCEED,
            throttle_->WillStartRequest());
  clock_.Advance(base::Milliseconds(2000));

  // Create a new throttle, this time without user gesture and interception.
  test_handle_->set_redirect_chain(
      std::vector<GURL>(3, GURL("http://foo.com")));
  EXPECT_CALL(*test_handle_, HasUserGesture()).WillRepeatedly(Return(false));
  throttle_ =
      std::make_unique<DataSharingNavigationThrottle>(*test_registry_);
  throttle_->SetServiceForTesting(&mock_collaboration_service_);
  DataSharingUtils::SetShouldInterceptForTesting(true);
  EXPECT_CALL(mock_collaboration_service_,
              HandleShareURLNavigationIntercepted(_, _, _))
      .Times(0);
  EXPECT_EQ(DataSharingNavigationThrottle::CANCEL,
            throttle_->WillStartRequest());
}

}  // namespace data_sharing
