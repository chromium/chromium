// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android/cross_device_signin_flow_navigation_throttle.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kValidBaseUrl[] = "https://www.google.com/chrome/go-mobile";

class CrossDeviceSigninFlowNavigationThrottleUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        switches::kCrossDeviceSignin, {{"url", kValidBaseUrl}});
  }

  content::NavigationThrottle* CreateAndGetThrottle(
      content::MockNavigationThrottleRegistry& registry) {
    CrossDeviceSigninFlowNavigationThrottle::MaybeCreateAndAdd(registry);
    if (registry.throttles().empty()) {
      return nullptr;
    }
    return registry.throttles().front().get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillStartRequest_CancelAndIgnoreOnValidDeepLink) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=0&email=test@gmail.com"),
      nullptr);
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);
  ASSERT_TRUE(throttle);
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest().action());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillStartRequest_ProceedOnInvalidDeepLink) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=invalid&email=test@gmail.com"),
      nullptr);
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);
  ASSERT_TRUE(throttle);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillStartRequest_ProceedOnNonDeepLinkUrl) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com/other-path"), nullptr);
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);
  ASSERT_TRUE(throttle);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillRedirectRequest_CancelAndIgnoreOnValidDeepLink) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=0&email=test@gmail.com"),
      nullptr);
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);
  ASSERT_TRUE(throttle);
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillRedirectRequest().action());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillRedirectRequest_ProceedOnInvalidDeepLink) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=invalid&email=test@gmail.com"),
      nullptr);
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);
  ASSERT_TRUE(throttle);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest().action());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillRedirectRequest_ProceedOnNonDeepLinkUrl) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com/other-path"), nullptr);
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);
  ASSERT_TRUE(throttle);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest().action());
}

class CrossDeviceSigninFlowNavigationThrottleDisabledUnitTest
    : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(switches::kCrossDeviceSignin);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CrossDeviceSigninFlowNavigationThrottleDisabledUnitTest,
       ThrottleNotRegistered) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=0&email=test@gmail.com"),
      nullptr);
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  CrossDeviceSigninFlowNavigationThrottle::MaybeCreateAndAdd(registry);
  EXPECT_TRUE(registry.throttles().empty());
}

}  // namespace
