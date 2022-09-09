// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceVariationsRestrictParameterPolicyTest
    : public DevicePolicyCrosBrowserTest {
 protected:
  DeviceVariationsRestrictParameterPolicyTest() = default;
  ~DeviceVariationsRestrictParameterPolicyTest() override = default;

  DeviceVariationsRestrictParameterPolicyTest(
      const DeviceVariationsRestrictParameterPolicyTest& other) = delete;
  DeviceVariationsRestrictParameterPolicyTest& operator=(
      const DeviceVariationsRestrictParameterPolicyTest& other) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Setup the device policy DeviceVariationsRestrictParameter.
    enterprise_management::ChromeDeviceSettingsProto& proto(
        device_policy()->payload());
    proto.mutable_variations_parameter()->set_parameter("restricted");

    RefreshDevicePolicy();
  }
};

IN_PROC_BROWSER_TEST_F(DeviceVariationsRestrictParameterPolicyTest,
                       VariationsURLValid) {
  const std::string default_variations_url =
      variations::VariationsService::GetDefaultVariationsServerURLForTesting();

  // Device policy has updated the cros settings.
  const GURL url =
      g_browser_process->variations_service()->GetVariationsServerURL(
          variations::VariationsService::HttpOptions::USE_HTTPS);
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("restricted", value);
}

class DeviceChromeVariationsPolicyTest : public DevicePolicyCrosBrowserTest {
 protected:
  DeviceChromeVariationsPolicyTest() = default;
  ~DeviceChromeVariationsPolicyTest() override = default;

  DeviceChromeVariationsPolicyTest(
      const DeviceChromeVariationsPolicyTest& other) = delete;
  DeviceChromeVariationsPolicyTest& operator=(
      const DeviceChromeVariationsPolicyTest& other) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Setup the device policy DeviceVariationsRestrictParameter.
    enterprise_management::ChromeDeviceSettingsProto& proto(
        device_policy()->payload());
    proto.mutable_device_chrome_variations_type()->set_value(2);

    RefreshDevicePolicy();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    // This makes sure that the the field trials are not initialized from a
    // testing config, taking into account the restrictions.
    command_line->AppendSwitch(
        variations::switches::kDisableFieldTrialTestingConfig);
  }

  base::HistogramTester histogram_tester_;
};

// It is expected that the first run of the browser that fetches a
// DeviceChromeVariations policy value does not apply it yet (because it is
// evaluated too early). The PRE_ test thus still expects NO_RESTRICTIONS.
IN_PROC_BROWSER_TEST_F(DeviceChromeVariationsPolicyTest, PRE_PolicyApplied) {
  // This uses the UMA histogram to verify the state of the PolicyRestriction at
  // the time it was evaluated (early in browser process initialization).
  histogram_tester_.ExpectUniqueSample(
      "Variations.PolicyRestriction",
      variations::RestrictionPolicy::NO_RESTRICTIONS, 1 /*count*/);
}

// This is after a restart of the chrome process - the restrictions now apply.
IN_PROC_BROWSER_TEST_F(DeviceChromeVariationsPolicyTest, PolicyApplied) {
  // This uses the UMA histogram to verify the state of the PolicyRestriction at
  // the time it was evaluated (early in browser process initialization).
  histogram_tester_.ExpectUniqueSample("Variations.PolicyRestriction",
                                       variations::RestrictionPolicy::ALL,
                                       1 /*count*/);
}

}  // namespace policy
