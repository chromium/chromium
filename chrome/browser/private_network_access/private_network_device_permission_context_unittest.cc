// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_network_access/private_network_device_permission_context.h"

#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/private_network_access/private_network_device_permission_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Optional;

namespace network {
namespace {

using Validity = PrivateNetworkDeviceValidity;
using Type = NewAcceptedDeviceType;

constexpr std::string_view kPrivateNetworkDeviceValidityHistogramName =
    "Security.PrivateNetworkAccess.PermissionDeviceValidity";
constexpr std::string_view kUserAcceptedPrivateNetworkDeviceHistogramName =
    "Security.PrivateNetworkAccess.PermissionNewAcceptedDeviceType";

class PrivateNetworkDevicePermissionContextTest : public testing::Test {
 public:
  PrivateNetworkDevicePermissionContextTest()
      : foo_url_("https://foo.com"),
        bar_url_("https://bar.com"),
        foo_origin_(url::Origin::Create(foo_url_)),
        bar_origin_(url::Origin::Create(bar_url_)),
        fake_device1_(blink::mojom::PrivateNetworkDevice::New(
            "test_id1",
            "test_name1",
            net::IPAddress(10, 0, 0, 1))),
        fake_device2_(blink::mojom::PrivateNetworkDevice::New(
            "test_id2",
            "test_name2",
            net::IPAddress(1, 2, 3, 4))) {}

  ~PrivateNetworkDevicePermissionContextTest() override = default;

  // Move-only class.
  PrivateNetworkDevicePermissionContextTest(
      const PrivateNetworkDevicePermissionContextTest&) = delete;
  PrivateNetworkDevicePermissionContextTest& operator=(
      const PrivateNetworkDevicePermissionContextTest&) = delete;

 protected:
  Profile* profile() { return &profile_; }

  PrivateNetworkDevicePermissionContext* GetChooserContext(Profile* profile) {
    auto* chooser_context =
        PrivateNetworkDevicePermissionContextFactory::GetForProfile(profile);
    return chooser_context;
  }

  const GURL foo_url_;
  const GURL bar_url_;
  const url::Origin foo_origin_;
  const url::Origin bar_origin_;
  blink::mojom::PrivateNetworkDevicePtr fake_device1_;
  blink::mojom::PrivateNetworkDevicePtr fake_device2_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(PrivateNetworkDevicePermissionContextTest, CheckGrantDevicePermission) {
  base::HistogramTester histogram_tester;
  PrivateNetworkDevicePermissionContext* context = GetChooserContext(profile());

  context->GrantDevicePermission(foo_origin_, *fake_device1_,
                                 /*is_device_valid=*/true);
  histogram_tester.ExpectUniqueSample(
      kUserAcceptedPrivateNetworkDeviceHistogramName, Type::kValidDevice, 1);

  context->GrantDevicePermission(foo_origin_, *fake_device2_,
                                 /*is_device_valid=*/false);
  histogram_tester.ExpectBucketCount(
      kUserAcceptedPrivateNetworkDeviceHistogramName, Type::kEphemeralDevice,
      1);
}

TEST_F(PrivateNetworkDevicePermissionContextTest, CheckHasDevicePermission) {
  base::HistogramTester histogram_tester;
  PrivateNetworkDevicePermissionContext* context = GetChooserContext(profile());

  context->HasDevicePermission(foo_origin_, *fake_device1_,
                               /*is_device_valid=*/true);
  histogram_tester.ExpectUniqueSample(
      kPrivateNetworkDeviceValidityHistogramName, Validity::kExistingDevice, 0);

  context->HasDevicePermission(foo_origin_, *fake_device2_,
                               /*is_device_valid=*/false);
  histogram_tester.ExpectUniqueSample(
      kPrivateNetworkDeviceValidityHistogramName, Validity::kExistingDevice, 0);

  context->GrantDevicePermission(foo_origin_, *fake_device1_,
                                 /*is_device_valid=*/true);
  context->HasDevicePermission(foo_origin_, *fake_device1_,
                               /*is_device_valid=*/true);
  context->HasDevicePermission(foo_origin_, *fake_device2_,
                               /*is_device_valid=*/true);
  histogram_tester.ExpectUniqueSample(
      kPrivateNetworkDeviceValidityHistogramName, Validity::kExistingDevice, 1);
}

}  // namespace
}  // namespace network
