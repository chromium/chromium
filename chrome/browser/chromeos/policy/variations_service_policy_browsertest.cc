// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/variations/service/variations_service.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class VariationsServiceDevicePolicyTest : public DevicePolicyCrosBrowserTest {
 protected:
  VariationsServiceDevicePolicyTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    SetSpecificDevicePolicies();
    RefreshDevicePolicy();
  }

  void SetSpecificDevicePolicies() {
    // Setup the device policy DeviceVariationsRestrictParameter.
    enterprise_management::ChromeDeviceSettingsProto& proto(
        device_policy()->payload());
    proto.mutable_variations_parameter()->set_parameter("restricted");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VariationsServiceDevicePolicyTest);
};

IN_PROC_BROWSER_TEST_F(VariationsServiceDevicePolicyTest, VariationsURLValid) {
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

}  // namespace policy
