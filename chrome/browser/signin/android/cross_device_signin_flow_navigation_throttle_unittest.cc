// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android/cross_device_signin_flow_navigation_throttle.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/browser/signin/android/signin_bridge_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/public/base/signin_deep_link_payload.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

namespace {

constexpr char kValidBaseUrl[] = "https://www.google.com/chrome/go-mobile";

class MockSigninBridge : public SigninBridge {
 public:
  MockSigninBridge() = default;

  MOCK_METHOD(void,
              StartSigninDeepLinkFlow,
              (ui::WindowAndroid * window,
               Profile* profile,
               const signin::SigninDeepLinkPayload& payload),
              (override));
};

std::unique_ptr<KeyedService> BuildMockSigninBridgeForTesting(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockSigninBridge>>();
}

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MOCK_METHOD(void, CloseContents, (content::WebContents*), (override));
};

}  // namespace

class CrossDeviceSigninFlowNavigationThrottleUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        switches::kCrossDeviceSignin, {{"url", kValidBaseUrl}});
    window_ = ui::WindowAndroid::CreateForTesting();
    window_->get()->AddChild(web_contents()->GetNativeView());
  }

  content::NavigationThrottle* CreateAndGetThrottle(
      content::MockNavigationThrottleRegistry& registry) {
    auto parser =
        signin::SigninDeepLinkParser::CreateForCrossDeviceSigninIfEnabled();
    CHECK(parser.has_value());
    throttle_ = base::WrapUnique(new CrossDeviceSigninFlowNavigationThrottle(
        registry, &mock_signin_bridge_, std::move(parser.value())));
    return throttle_.get();
  }

  MockSigninBridge* signin_bridge() { return &mock_signin_bridge_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockSigninBridge> mock_signin_bridge_;
  std::unique_ptr<CrossDeviceSigninFlowNavigationThrottle> throttle_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
};

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillStartRequest_CancelAndIgnoreOnValidDeepLink) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=0&email=test@gmail.com"),
      main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);

  ASSERT_TRUE(throttle);
  signin::SigninDeepLinkPayload expected_payload{
      .entry_point_id = signin::ExternalEntryPoint::kUnknown,
      .entry_point_id_raw_value_for_metrics = 0,
      .email = "test@gmail.com"};
  EXPECT_CALL(*signin_bridge(),
              StartSigninDeepLinkFlow(web_contents()->GetTopLevelNativeWindow(),
                                      profile(), expected_payload))
      .Times(1);
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest().action());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillStartRequest_ProceedOnInvalidDeepLink) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=invalid&email=test@gmail.com"),
      main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);

  ASSERT_TRUE(throttle);
  EXPECT_CALL(*signin_bridge(),
              StartSigninDeepLinkFlow(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillStartRequest_ProceedOnNonDeepLinkUrl) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com/other-path"), main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);

  ASSERT_TRUE(throttle);
  EXPECT_CALL(*signin_bridge(),
              StartSigninDeepLinkFlow(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillRedirectRequest_CancelAndIgnoreOnValidDeepLink) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=0&email=test@gmail.com"),
      main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);

  ASSERT_TRUE(throttle);
  signin::SigninDeepLinkPayload expected_payload{
      .entry_point_id = signin::ExternalEntryPoint::kUnknown,
      .entry_point_id_raw_value_for_metrics = 0,
      .email = "test@gmail.com"};
  EXPECT_CALL(*signin_bridge(),
              StartSigninDeepLinkFlow(web_contents()->GetTopLevelNativeWindow(),
                                      profile(), expected_payload))
      .Times(1);
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillRedirectRequest().action());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillRedirectRequest_ProceedOnInvalidDeepLink) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=invalid&email=test@gmail.com"),
      main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);

  ASSERT_TRUE(throttle);
  EXPECT_CALL(*signin_bridge(),
              StartSigninDeepLinkFlow(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest().action());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleUnitTest,
       WillRedirectRequest_ProceedOnNonDeepLinkUrl) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://example.com/other-path"), main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);

  ASSERT_TRUE(throttle);
  EXPECT_CALL(*signin_bridge(),
              StartSigninDeepLinkFlow(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest().action());
}

class CrossDeviceSigninFlowNavigationThrottleFactoryUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        switches::kCrossDeviceSignin, {{"url", kValidBaseUrl}});
    ChromeRenderViewHostTestHarness::SetUp();
    window_ = ui::WindowAndroid::CreateForTesting();
    window_->get()->AddChild(web_contents()->GetNativeView());
    SigninBridgeFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating(&BuildMockSigninBridgeForTesting));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
};

TEST_F(CrossDeviceSigninFlowNavigationThrottleFactoryUnitTest,
       ThrottleRegistered) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=0&email=test@gmail.com"),
      main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  CrossDeviceSigninFlowNavigationThrottle::MaybeCreateAndAdd(registry);
  EXPECT_FALSE(registry.throttles().empty());
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleFactoryUnitTest,
       ThrottleNotRegisteredForIncognito) {
  auto* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto mock_web_contents =
      content::WebContentsTester::CreateTestWebContents(otr_profile, nullptr);
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=0&email=test@gmail.com"),
      mock_web_contents->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  CrossDeviceSigninFlowNavigationThrottle::MaybeCreateAndAdd(registry);
  EXPECT_TRUE(registry.throttles().empty());
}

class CrossDeviceSigninFlowNavigationThrottleFactoryDisabledUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(switches::kCrossDeviceSignin);
    ChromeRenderViewHostTestHarness::SetUp();
    window_ = ui::WindowAndroid::CreateForTesting();
    window_->get()->AddChild(web_contents()->GetNativeView());
    SigninBridgeFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating(&BuildMockSigninBridgeForTesting));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
};

TEST_F(CrossDeviceSigninFlowNavigationThrottleFactoryDisabledUnitTest,
       ThrottleNotRegistered) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=0&email=test@gmail.com"),
      main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  CrossDeviceSigninFlowNavigationThrottle::MaybeCreateAndAdd(registry);
  EXPECT_TRUE(registry.throttles().empty());
}

class CrossDeviceSigninFlowNavigationThrottleTabClosingUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        switches::kCrossDeviceSignin, {{"url", kValidBaseUrl}});
    window_ = ui::WindowAndroid::CreateForTesting();
    window_->get()->AddChild(web_contents()->GetNativeView());
    web_contents()->SetDelegate(&mock_web_contents_delegate_);
  }

  content::NavigationThrottle* CreateAndGetThrottle(
      content::MockNavigationThrottleRegistry& registry) {
    auto parser =
        signin::SigninDeepLinkParser::CreateForCrossDeviceSigninIfEnabled();
    CHECK(parser.has_value());
    throttle_ = base::WrapUnique(new CrossDeviceSigninFlowNavigationThrottle(
        registry, &mock_signin_bridge_, std::move(parser.value())));
    return throttle_.get();
  }

  void TearDown() override {
    web_contents()->SetDelegate(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockWebContentsDelegate* web_contents_delegate() {
    return &mock_web_contents_delegate_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockSigninBridge> mock_signin_bridge_;
  std::unique_ptr<CrossDeviceSigninFlowNavigationThrottle> throttle_;
  MockWebContentsDelegate mock_web_contents_delegate_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
};

TEST_F(CrossDeviceSigninFlowNavigationThrottleTabClosingUnitTest,
       ClosePageIfTabIsEmpty) {
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=0&email=test@gmail.com"),
      main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);

  ASSERT_TRUE(throttle);
  base::RunLoop run_loop;
  EXPECT_CALL(*web_contents_delegate(), CloseContents(web_contents()))
      .WillOnce([&run_loop](content::WebContents*) { run_loop.Quit(); });
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest().action());
  run_loop.Run();
}

TEST_F(CrossDeviceSigninFlowNavigationThrottleTabClosingUnitTest,
       NoClosePageIfTabIsNotEmpty) {
  NavigateAndCommit(GURL("https://www.example.com"));
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://www.google.com/chrome/"
           "go-mobile?entry_point_id=0&email=test@gmail.com"),
      main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  auto* throttle = CreateAndGetThrottle(registry);

  ASSERT_TRUE(throttle);
  EXPECT_CALL(*web_contents_delegate(), CloseContents(testing::_)).Times(0);
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest().action());
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}
