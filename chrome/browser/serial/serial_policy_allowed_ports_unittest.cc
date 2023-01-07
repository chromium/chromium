// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_policy_allowed_ports.h"

#include "base/containers/contains.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ::base::test::ParseJson;
using ::testing::UnorderedElementsAre;

device::mojom::SerialPortInfoPtr CreateUsbDevice(uint16_t vendor_id,
                                                 uint16_t product_id) {
  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->has_vendor_id = true;
  port->vendor_id = vendor_id;
  port->has_product_id = true;
  port->product_id = product_id;
  return port;
}

device::mojom::SerialPortInfoPtr CreatePlatformPort() {
  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  return port;
}

}  // namespace

class SerialPolicyAllowedPortsTest : public testing::Test {
 public:
  SerialPolicyAllowedPortsTest() {
    RegisterLocalState(local_state_.registry());
  }

  ~SerialPolicyAllowedPortsTest() override = default;

  void SetAllowAllPortsForUrlsPrefValue(const base::Value& value) {
    local_state_.Set(prefs::kManagedSerialAllowAllPortsForUrls, value);
  }

  void SetAllowUsbDevicesForUrlsPrefValue(const base::Value& value) {
    local_state_.Set(prefs::kManagedSerialAllowUsbDevicesForUrls, value);
  }

  void InitializePolicy() {
    EXPECT_FALSE(policy_);
    policy_ = std::make_unique<SerialPolicyAllowedPorts>(&local_state_);
  }

 protected:
  SerialPolicyAllowedPorts* policy() { return policy_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<SerialPolicyAllowedPorts> policy_;
};

TEST_F(SerialPolicyAllowedPortsTest, InitializeWithMissingPrefValue) {
  InitializePolicy();
  EXPECT_TRUE(policy()->usb_device_policy().empty());
  EXPECT_TRUE(policy()->usb_vendor_policy().empty());
  EXPECT_TRUE(policy()->all_ports_policy().empty());
}

TEST_F(SerialPolicyAllowedPortsTest, InitializeWithEmptyPrefValues) {
  SetAllowAllPortsForUrlsPrefValue(base::Value(base::Value::Type::LIST));
  SetAllowUsbDevicesForUrlsPrefValue(base::Value(base::Value::Type::LIST));

  InitializePolicy();
  EXPECT_TRUE(policy()->usb_device_policy().empty());
  EXPECT_TRUE(policy()->usb_vendor_policy().empty());
  EXPECT_TRUE(policy()->all_ports_policy().empty());
}

TEST_F(SerialPolicyAllowedPortsTest, InitializeWithPrefValues) {
  constexpr char kAllPortsPolicySetting[] = R"(["https://www.youtube.com"])";
  constexpr char kUsbDevicesPolicySetting[] = R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 },
          { "vendor_id": 4321 }
        ],
        "urls": [
          "https://google.com",
          "https://crbug.com"
        ]
      }
    ])";
  const auto kYoutubeOrigin =
      url::Origin::Create(GURL("https://www.youtube.com"));
  const auto kGoogleOrigin = url::Origin::Create(GURL("https://google.com"));
  const auto kCrbugOrigin = url::Origin::Create(GURL("https://crbug.com"));

  SetAllowAllPortsForUrlsPrefValue(ParseJson(kAllPortsPolicySetting));
  SetAllowUsbDevicesForUrlsPrefValue(ParseJson(kUsbDevicesPolicySetting));
  InitializePolicy();

  EXPECT_EQ(1u, policy()->usb_device_policy().size());
  EXPECT_EQ(1u, policy()->usb_vendor_policy().size());
  EXPECT_EQ(1u, policy()->all_ports_policy().size());

  EXPECT_THAT(policy()->all_ports_policy(),
              UnorderedElementsAre(kYoutubeOrigin));

  const auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(policy()->usb_device_policy(), device_key));
  EXPECT_THAT(policy()->usb_device_policy().at(device_key),
              UnorderedElementsAre(kGoogleOrigin, kCrbugOrigin));

  ASSERT_TRUE(base::Contains(policy()->usb_vendor_policy(), 4321));
  EXPECT_THAT(policy()->usb_vendor_policy().at(4321),
              UnorderedElementsAre(kGoogleOrigin, kCrbugOrigin));

  auto vendor1_device = CreateUsbDevice(1234, 5678);
  EXPECT_TRUE(policy()->HasPortPermission(kYoutubeOrigin, *vendor1_device));
  EXPECT_TRUE(policy()->HasPortPermission(kGoogleOrigin, *vendor1_device));
  EXPECT_TRUE(policy()->HasPortPermission(kCrbugOrigin, *vendor1_device));

  auto vendor1_alternate_device = CreateUsbDevice(1234, 1234);
  EXPECT_TRUE(
      policy()->HasPortPermission(kYoutubeOrigin, *vendor1_alternate_device));
  EXPECT_FALSE(
      policy()->HasPortPermission(kGoogleOrigin, *vendor1_alternate_device));
  EXPECT_FALSE(
      policy()->HasPortPermission(kCrbugOrigin, *vendor1_alternate_device));

  auto vendor2_device = CreateUsbDevice(4321, 1234);
  EXPECT_TRUE(policy()->HasPortPermission(kYoutubeOrigin, *vendor2_device));
  EXPECT_TRUE(policy()->HasPortPermission(kGoogleOrigin, *vendor2_device));
  EXPECT_TRUE(policy()->HasPortPermission(kCrbugOrigin, *vendor2_device));

  auto platform_port = CreatePlatformPort();
  EXPECT_TRUE(policy()->HasPortPermission(kYoutubeOrigin, *platform_port));
  EXPECT_FALSE(policy()->HasPortPermission(kGoogleOrigin, *platform_port));
  EXPECT_FALSE(policy()->HasPortPermission(kCrbugOrigin, *platform_port));
}

TEST_F(SerialPolicyAllowedPortsTest,
       InitializeWithMissingPrefValuesThenUpdate) {
  InitializePolicy();

  constexpr char kAllPortsPolicySetting[] = R"(["https://www.youtube.com"])";
  constexpr char kUsbDevicesPolicySetting[] = R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 },
          { "vendor_id": 4321 }
        ],
        "urls": [
          "https://google.com",
          "https://crbug.com"
        ]
      }
    ])";
  const auto kYoutubeOrigin =
      url::Origin::Create(GURL("https://www.youtube.com"));
  const auto kGoogleOrigin = url::Origin::Create(GURL("https://google.com"));
  const auto kCrbugOrigin = url::Origin::Create(GURL("https://crbug.com"));

  SetAllowAllPortsForUrlsPrefValue(ParseJson(kAllPortsPolicySetting));
  SetAllowUsbDevicesForUrlsPrefValue(ParseJson(kUsbDevicesPolicySetting));

  EXPECT_EQ(1u, policy()->usb_device_policy().size());
  EXPECT_EQ(1u, policy()->usb_vendor_policy().size());
  EXPECT_EQ(1u, policy()->all_ports_policy().size());

  EXPECT_THAT(policy()->all_ports_policy(),
              UnorderedElementsAre(kYoutubeOrigin));

  const auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(policy()->usb_device_policy(), device_key));
  EXPECT_THAT(policy()->usb_device_policy().at(device_key),
              UnorderedElementsAre(kGoogleOrigin, kCrbugOrigin));

  ASSERT_TRUE(base::Contains(policy()->usb_vendor_policy(), 4321));
  EXPECT_THAT(policy()->usb_vendor_policy().at(4321),
              UnorderedElementsAre(kGoogleOrigin, kCrbugOrigin));

  auto vendor1_device = CreateUsbDevice(1234, 5678);
  EXPECT_TRUE(policy()->HasPortPermission(kYoutubeOrigin, *vendor1_device));
  EXPECT_TRUE(policy()->HasPortPermission(kGoogleOrigin, *vendor1_device));
  EXPECT_TRUE(policy()->HasPortPermission(kCrbugOrigin, *vendor1_device));

  auto vendor1_alternate_device = CreateUsbDevice(1234, 1234);
  EXPECT_TRUE(
      policy()->HasPortPermission(kYoutubeOrigin, *vendor1_alternate_device));
  EXPECT_FALSE(
      policy()->HasPortPermission(kGoogleOrigin, *vendor1_alternate_device));
  EXPECT_FALSE(
      policy()->HasPortPermission(kCrbugOrigin, *vendor1_alternate_device));

  auto vendor2_device = CreateUsbDevice(4321, 1234);
  EXPECT_TRUE(policy()->HasPortPermission(kYoutubeOrigin, *vendor2_device));
  EXPECT_TRUE(policy()->HasPortPermission(kGoogleOrigin, *vendor2_device));
  EXPECT_TRUE(policy()->HasPortPermission(kCrbugOrigin, *vendor2_device));

  auto platform_port = CreatePlatformPort();
  EXPECT_TRUE(policy()->HasPortPermission(kYoutubeOrigin, *platform_port));
  EXPECT_FALSE(policy()->HasPortPermission(kGoogleOrigin, *platform_port));
  EXPECT_FALSE(policy()->HasPortPermission(kCrbugOrigin, *platform_port));
}

TEST_F(SerialPolicyAllowedPortsTest, InitializeWithPrefValuesThenRemovePolicy) {
  constexpr char kAllPortsPolicySetting[] = R"(["https://www.youtube.com"])";
  constexpr char kUsbDevicesPolicySetting[] = R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 },
          { "vendor_id": 4321 }
        ],
        "urls": [
          "https://google.com",
          "https://crbug.com"
        ]
      }
    ])";

  SetAllowAllPortsForUrlsPrefValue(ParseJson(kAllPortsPolicySetting));
  SetAllowUsbDevicesForUrlsPrefValue(ParseJson(kUsbDevicesPolicySetting));
  InitializePolicy();

  SetAllowAllPortsForUrlsPrefValue(base::Value(base::Value::Type::LIST));
  SetAllowUsbDevicesForUrlsPrefValue(base::Value(base::Value::Type::LIST));
  EXPECT_TRUE(policy()->usb_device_policy().empty());
  EXPECT_TRUE(policy()->usb_vendor_policy().empty());
  EXPECT_TRUE(policy()->all_ports_policy().empty());
}

TEST_F(SerialPolicyAllowedPortsTest, MultipleItemsWithOverlap) {
  constexpr char kUsbDevicesPolicySetting[] = R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 },
          { "vendor_id": 4321 }
        ],
        "urls": [
          "https://google.com",
          "https://crbug.com"
        ]
      },
      {
        "devices": [
          { "vendor_id": 1234 },
          { "vendor_id": 4321, "product_id": 8765 }
        ],
        "urls": [
          "https://crbug.com",
          "https://www.youtube.com"
        ]
      }
    ])";
  const auto kGoogleOrigin = url::Origin::Create(GURL("https://google.com"));
  const auto kCrbugOrigin = url::Origin::Create(GURL("https://crbug.com"));
  const auto kYoutubeOrigin =
      url::Origin::Create(GURL("https://www.youtube.com"));

  SetAllowUsbDevicesForUrlsPrefValue(ParseJson(kUsbDevicesPolicySetting));
  InitializePolicy();

  EXPECT_EQ(2u, policy()->usb_device_policy().size());
  EXPECT_EQ(2u, policy()->usb_vendor_policy().size());
  EXPECT_EQ(0u, policy()->all_ports_policy().size());

  const auto device1_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(policy()->usb_device_policy(), device1_key));
  EXPECT_THAT(policy()->usb_device_policy().at(device1_key),
              UnorderedElementsAre(kGoogleOrigin, kCrbugOrigin));

  const auto device2_key = std::make_pair(4321, 8765);
  ASSERT_TRUE(base::Contains(policy()->usb_device_policy(), device2_key));
  EXPECT_THAT(policy()->usb_device_policy().at(device2_key),
              UnorderedElementsAre(kCrbugOrigin, kYoutubeOrigin));

  ASSERT_TRUE(base::Contains(policy()->usb_vendor_policy(), 1234));
  EXPECT_THAT(policy()->usb_vendor_policy().at(1234),
              UnorderedElementsAre(kCrbugOrigin, kYoutubeOrigin));

  ASSERT_TRUE(base::Contains(policy()->usb_vendor_policy(), 4321));
  EXPECT_THAT(policy()->usb_vendor_policy().at(4321),
              UnorderedElementsAre(kGoogleOrigin, kCrbugOrigin));

  auto vendor1_device1 = CreateUsbDevice(1234, 5678);
  EXPECT_TRUE(policy()->HasPortPermission(kGoogleOrigin, *vendor1_device1));
  EXPECT_TRUE(policy()->HasPortPermission(kCrbugOrigin, *vendor1_device1));
  EXPECT_TRUE(policy()->HasPortPermission(kYoutubeOrigin, *vendor1_device1));

  auto vendor1_alternate_device = CreateUsbDevice(1234, 1234);
  EXPECT_FALSE(
      policy()->HasPortPermission(kGoogleOrigin, *vendor1_alternate_device));
  EXPECT_TRUE(
      policy()->HasPortPermission(kCrbugOrigin, *vendor1_alternate_device));
  EXPECT_TRUE(
      policy()->HasPortPermission(kYoutubeOrigin, *vendor1_alternate_device));

  auto vendor2_device = CreateUsbDevice(4321, 8765);
  EXPECT_TRUE(policy()->HasPortPermission(kGoogleOrigin, *vendor2_device));
  EXPECT_TRUE(policy()->HasPortPermission(kCrbugOrigin, *vendor2_device));
  EXPECT_TRUE(policy()->HasPortPermission(kYoutubeOrigin, *vendor2_device));

  auto vendor2_alternate_device = CreateUsbDevice(4321, 1234);
  EXPECT_TRUE(
      policy()->HasPortPermission(kGoogleOrigin, *vendor2_alternate_device));
  EXPECT_TRUE(
      policy()->HasPortPermission(kCrbugOrigin, *vendor2_alternate_device));
  EXPECT_FALSE(
      policy()->HasPortPermission(kYoutubeOrigin, *vendor2_alternate_device));

  auto vendor3_device = CreateUsbDevice(9012, 3456);
  EXPECT_FALSE(policy()->HasPortPermission(kGoogleOrigin, *vendor3_device));
  EXPECT_FALSE(policy()->HasPortPermission(kCrbugOrigin, *vendor3_device));
  EXPECT_FALSE(policy()->HasPortPermission(kYoutubeOrigin, *vendor3_device));

  auto platform_port = CreatePlatformPort();
  EXPECT_FALSE(policy()->HasPortPermission(kGoogleOrigin, *platform_port));
  EXPECT_FALSE(policy()->HasPortPermission(kCrbugOrigin, *platform_port));
  EXPECT_FALSE(policy()->HasPortPermission(kYoutubeOrigin, *platform_port));
}
