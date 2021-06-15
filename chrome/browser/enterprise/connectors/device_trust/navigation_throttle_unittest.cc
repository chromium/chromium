// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"

#include "base/values.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationThrottle;

namespace {

const base::Value kOrigins[]{base::Value("https://www.example.com"),
                             base::Value("example2.example.com")};

constexpr char challenge[] =
    "{"
    "\"challenge\": {"
    " \"data\": "
    "\"ChZFbnRlcnByaXNlS2V5Q2hhbGxlbmdlEiB+CPt6kzZVCmxIPc4K5NdVGsTLYVcA0ekaCVq+"
    "8KbZEBif4oXClC8=\","
    "  \"signature\": "
    "\"TiR/Qd/f+V/XFnYPIqeLO6/AXI+SKOnKGJmqhJd06MjnHhRnCK5u/BdFkq2H5U/"
    "qqAx4DS6SfcLfJ+6NEdsemn/5UTmarOOxWA8Fh2zc2a2Zr1+MGDdgRkckIzA5iw99/"
    "EV+xIUXyVaqJjSuD9iPSFJzlJUtlhbijf8JT1w8PuuxNOERuhqIrJvpZFpb+"
    "u99YLuGrpw7y64Bh6AhsXryGjowqXYojYWAOYeHX4b2axkHDsThybI+v+"
    "ECtVHi3l6Z2TOwr7fkyhoy1Kz9swd30rw6/VDB92jzrJTQoy3rQ2+aY8KxycU/"
    "nuJn3H6583SsiaTbKgyHKmObbGdt0GVWLQ==\""
    "}"
    "}";

}  // namespace

namespace enterprise_connectors {

class DeviceTrustNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  DeviceTrustNavigationThrottleTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    OSCryptMocker::SetUp();
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    OSCryptMocker::TearDown();
  }

  void EnableDeviceTrust() {
    Profile::FromBrowserContext(browser_context())
        ->GetPrefs()
        ->Set(kContextAwareAccessSignalsAllowlistPref,
              base::ListValue(kOrigins));
  }

 private:
  ScopedTestingLocalState testing_local_state_;
};

TEST_F(DeviceTrustNavigationThrottleTest, ExpectHeaderDeviceTrustOnRequest) {
  EnableDeviceTrust();
  GURL url("https://www.example.com/");
  content::MockNavigationHandle test_handle(url, main_rfh());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(1);
  auto throttle =
      DeviceTrustNavigationThrottle::MaybeCreateThrottleFor(&test_handle);
  ASSERT_TRUE(throttle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

TEST_F(DeviceTrustNavigationThrottleTest, NoHeaderDeviceTrustOnRequest) {
  EnableDeviceTrust();
  GURL url("https://www.no-example.com/");
  content::MockNavigationHandle test_handle(url, main_rfh());
  EXPECT_CALL(test_handle, SetRequestHeader("X-Device-Trust", "VerifiedAccess"))
      .Times(0);
  auto throttle =
      DeviceTrustNavigationThrottle::MaybeCreateThrottleFor(&test_handle);
  ASSERT_TRUE(throttle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

scoped_refptr<net::HttpResponseHeaders> GetHeaderChallenge(
    const std::string& challenge) {
  std::string raw_response_headers =
      "HTTP/1.1 200 OK\r\n"
      "x-verified-access-challenge: " +
      challenge + "\r\n";
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_response_headers));
}

TEST_F(DeviceTrustNavigationThrottleTest, BuildChallengeResponseFromHeader) {
  EnableDeviceTrust();
  GURL url("https://www.example.com/");
  content::MockNavigationHandle test_handle(url, main_rfh());

  test_handle.set_response_headers(GetHeaderChallenge(challenge));
  auto throttle =
      DeviceTrustNavigationThrottle::MaybeCreateThrottleFor(&test_handle);
  ASSERT_TRUE(throttle);

  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());
}

}  // namespace enterprise_connectors
