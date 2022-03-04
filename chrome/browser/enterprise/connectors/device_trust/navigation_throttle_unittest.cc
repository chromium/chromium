// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/fake_device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/mock_device_trust_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationThrottle;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {

base::Value::ListStorage GetTrustedUrls() {
  base::Value::ListStorage trusted_urls;
  trusted_urls.push_back(base::Value("https://www.example.com"));
  trusted_urls.push_back(base::Value("example2.example.com"));
  return trusted_urls;
}

constexpr char kChallenge[] = R"({"challenge": "encrypted_challenge_string"})";

scoped_refptr<net::HttpResponseHeaders> GetHeaderChallenge(
    const std::string& challenge) {
  std::string raw_response_headers =
      "HTTP/1.1 200 OK\r\n"
      "x-verified-access-challenge: " +
      challenge + "\r\n";
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_response_headers));
}

}  // namespace

namespace enterprise_connectors {

class DeviceTrustNavigationThrottleTest : public testing::Test {
 public:
  DeviceTrustNavigationThrottleTest() : trusted_urls_(GetTrustedUrls()) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kDeviceTrustConnectorEnabled);
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    fake_connector_ = std::make_unique<FakeDeviceTrustConnectorService>(
        profile_.GetTestingPrefService());
    fake_connector_->Initialize();

    fake_connector_->update_policy(
        std::make_unique<base::ListValue>(GetTrustedUrls()));

    EXPECT_CALL(mock_device_trust_service_, Watches(_))
        .WillRepeatedly(Invoke(
            [this](const GURL& url) { return fake_connector_->Watches(url); }));
    EXPECT_CALL(mock_device_trust_service_, IsEnabled())
        .WillRepeatedly(Return(true));
  }

  std::unique_ptr<DeviceTrustNavigationThrottle> CreateThrottle(
      content::NavigationHandle* navigation_handle) {
    return std::make_unique<DeviceTrustNavigationThrottle>(
        &mock_device_trust_service_, navigation_handle);
  }

  content::WebContents* web_contents() const { return web_contents_.get(); }
  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetMainFrame();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  test::MockDeviceTrustService mock_device_trust_service_;
  std::unique_ptr<FakeDeviceTrustConnectorService> fake_connector_;
  base::ListValue trusted_urls_;
};

TEST_F(DeviceTrustNavigationThrottleTest, ExpectHeaderDeviceTrustOnRequest) {
  content::MockNavigationHandle test_handle(GURL("https://www.example.com/"),
                                            main_frame());
  EXPECT_CALL(test_handle,
              SetRequestHeader("X-Device-Trust", "VerifiedAccess"));
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

TEST_F(DeviceTrustNavigationThrottleTest, NullService) {
  content::MockNavigationHandle test_handle(GURL("https://www.example.com/"),
                                            main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle =
      std::make_unique<DeviceTrustNavigationThrottle>(nullptr, &test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

TEST_F(DeviceTrustNavigationThrottleTest, NoHeaderDeviceTrustOnRequest) {
  content::MockNavigationHandle test_handle(GURL("https://www.no-example.com/"),
                                            main_frame());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

TEST_F(DeviceTrustNavigationThrottleTest, BuildChallengeResponseFromHeader) {
  content::MockNavigationHandle test_handle(GURL("https://www.example.com/"),
                                            main_frame());

  test_handle.set_response_headers(GetHeaderChallenge(kChallenge));
  auto throttle = CreateThrottle(&test_handle);

  EXPECT_CALL(test_handle, RemoveRequestHeader("X-Device-Trust"));
  EXPECT_CALL(mock_device_trust_service_,
              BuildChallengeResponse(kChallenge, _));

  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());

  base::RunLoop().RunUntilIdle();
}

}  // namespace enterprise_connectors
