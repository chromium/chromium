// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/installer_policies/aw_package_names_allowlist_component_installer_policy.h"

#include <vector>

#include "android_webview/common/aw_switches.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

const uint8_t kWebViewAppsPackageNamesAllowlistPublicKeySHA256[32] = {
    0x04, 0xcb, 0xb8, 0xd5, 0xf9, 0x36, 0x2b, 0x36, 0x04, 0xb2, 0x60,
    0xaf, 0x9c, 0x04, 0xa1, 0x08, 0xa3, 0xe9, 0xdc, 0x92, 0x46, 0xe7,
    0xae, 0xc8, 0x3e, 0x32, 0x6f, 0x74, 0x43, 0x02, 0xf3, 0x7e};

class AwPackageNamesAllowlistComponentInstallerPolicyTest
    : public ::testing::Test {
 public:
  AwPackageNamesAllowlistComponentInstallerPolicyTest() = default;

 protected:
  base::test::TaskEnvironment env_;
};

// TODO(crbug.com/1202702): Add a test that calls
// RegisterWebViewAppsPackageNamesAllowlistComponent() and checks that
// registration_finished is called.

TEST_F(AwPackageNamesAllowlistComponentInstallerPolicyTest, ComponentHash) {
  auto policy =
      std::make_unique<AwPackageNamesAllowlistComponentInstallerPolicy>();

  std::vector<uint8_t> expected;
  expected.assign(
      kWebViewAppsPackageNamesAllowlistPublicKeySHA256,
      kWebViewAppsPackageNamesAllowlistPublicKeySHA256 +
          std::size(kWebViewAppsPackageNamesAllowlistPublicKeySHA256));

  std::vector<uint8_t> actual;
  policy->GetHash(&actual);

  EXPECT_EQ(expected, actual);

  std::string expected_id = "aemllinfpjdgcldgaelcgakpjmaekbai";
  std::string actual_id = update_client::GetCrxIdFromPublicKeyHash(actual);

  EXPECT_EQ(expected_id, actual_id);
}

}  // namespace android_webview
