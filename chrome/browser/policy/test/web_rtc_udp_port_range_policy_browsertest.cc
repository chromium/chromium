// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"

#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Arbitrary port range for testing the WebRTC UDP port policy.
const char kTestWebRtcUdpPortRange[] = "10000-10100";

}  // namespace

// Sets the proper policy before the browser is started.
template <bool enable>
class WebRtcUdpPortRangePolicyTest : public PolicyTest {
 public:
  WebRtcUdpPortRangePolicyTest() = default;
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    if (enable) {
      policies.Set(key::kWebRtcUdpPortRange, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                   base::Value(kTestWebRtcUdpPortRange), nullptr);
    }
    provider_.UpdateChromePolicy(policies);
  }
};

using WebRtcUdpPortRangeEnabledPolicyTest = WebRtcUdpPortRangePolicyTest<true>;
using WebRtcUdpPortRangeDisabledPolicyTest =
    WebRtcUdpPortRangePolicyTest<false>;

IN_PROC_BROWSER_TEST_F(WebRtcUdpPortRangeEnabledPolicyTest,
                       WebRtcUdpPortRangeEnabled) {
  std::string port_range;
  const PrefService::Preference* pref =
      user_prefs::UserPrefs::Get(chrome_test_utils::GetProfile(this))
          ->FindPreference(prefs::kWebRTCUDPPortRange);
  if (pref->GetValue()->is_string())
    port_range = pref->GetValue()->GetString();
  EXPECT_EQ(kTestWebRtcUdpPortRange, port_range);
}

IN_PROC_BROWSER_TEST_F(WebRtcUdpPortRangeDisabledPolicyTest,
                       WebRtcUdpPortRangeDisabled) {
  std::string port_range;
  const PrefService::Preference* pref =
      user_prefs::UserPrefs::Get(chrome_test_utils::GetProfile(this))
          ->FindPreference(prefs::kWebRTCUDPPortRange);
  if (pref->GetValue()->is_string())
    port_range = pref->GetValue()->GetString();
  EXPECT_TRUE(port_range.empty());
}

}  // namespace policy
