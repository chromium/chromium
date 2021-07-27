// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"

#include "base/values.h"
#include "build/build_config.h"
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

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
constexpr char challenge[] =
    "{"
    "\"challenge\": "
    "\"CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIELlPXqh8+"
    "rZJ2VIqwPXtPFrr653QdRrIzHFwqP+"
    "b3L8GJTcufirLxKAAkindNwTfwYUcbCFDjiW3kXdmDPE0wC0J6b5ZI6X6vOVcSMXTpK7nxsAGK"
    "zFV+i80LCnfwUZn7Ne1bHzloAqBdpLOu53vQ63hKRk6MRPhc9jYVDsvqXfQ7s+"
    "FUA5r3lxdoluxwAUMFqcP4VgnMvKzKTPYbnnB+xj5h5BZqjQToXJYoP4VC3/"
    "ID+YHNsCWy5o7+G5jnq0ak3zeqWfo1+lCibMPsCM+"
    "2g7nCZIwvwWlfoKwv3aKvOVMBcJxPAIxH1w+hH+"
    "NWxqRi6qgZm84q0ylm0ybs6TFjdgLvSViAIp0Z9p/An/"
    "u3W4CMboCswxIxNYRCGrIIVPElE3Yb4QS65mKrg=\""
    "}";
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

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

// TODO(b/194041030): Enable for Chrome OS after navigation is deferred before
// the challenge response is created.
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
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
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

}  // namespace enterprise_connectors
