// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_policy_allowed_devices.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ::base::test::ParseJson;
using ::testing::UnorderedElementsAre;

class HidPolicyAllowedDevicesTest : public testing::Test {
 public:
  HidPolicyAllowedDevicesTest() { RegisterLocalState(local_state_.registry()); }

  ~HidPolicyAllowedDevicesTest() override = default;

  void InitializePolicy(bool on_login_screen = false) {
    EXPECT_FALSE(policy_);
    policy_ = std::make_unique<HidPolicyAllowedDevices>(&local_state_,
                                                        on_login_screen);
  }

  void SetAllowDevicesForUrlsPrefValue(std::string_view policy) {
    local_state_.Set(prefs::kManagedWebHidAllowDevicesForUrls,
                     ParseJson(policy));
  }

  void SetAllowDevicesForUrlsOnLoginScreenPrefValue(std::string_view policy) {
    local_state_.Set(prefs::kManagedWebHidAllowDevicesForUrlsOnLoginScreen,
                     ParseJson(policy));
  }

  void SetAllowDevicesWithHidUsagesForUrlsPrefValue(std::string_view policy) {
    local_state_.Set(prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
                     ParseJson(policy));
  }

  void SetAllowAllDevicesForUrlsPrefValue(std::string_view policy) {
    local_state_.Set(prefs::kManagedWebHidAllowAllDevicesForUrls,
                     ParseJson(policy));
  }

 protected:
  device::FakeHidManager& hid_manager() { return hid_manager_; }
  HidPolicyAllowedDevices* policy() { return policy_.get(); }

  device::mojom::HidDeviceInfoPtr CreateAndAddDevice(uint16_t vendor_id,
                                                     uint16_t product_id,
                                                     uint16_t usage_page,
                                                     uint16_t usage) {
    static int next_physical_device_id = 0;
    return hid_manager_.CreateAndAddDeviceWithTopLevelUsage(
        base::NumberToString(next_physical_device_id++), vendor_id, product_id,
        "product-name", "serial-number",
        device::mojom::HidBusType::kHIDBusTypeUSB, usage_page, usage);
  }

 private:
  device::FakeHidManager hid_manager_;
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<HidPolicyAllowedDevices> policy_;
};

}  // namespace

TEST_F(HidPolicyAllowedDevicesTest, InitializeWithMissingPrefValue) {
  InitializePolicy();

  EXPECT_EQ(0u, policy()->all_devices_policy().size());
  EXPECT_EQ(0u, policy()->vendor_policy().size());
  EXPECT_EQ(0u, policy()->device_policy().size());
  EXPECT_EQ(0u, policy()->usage_page_policy().size());
  EXPECT_EQ(0u, policy()->usage_policy().size());
}

TEST_F(HidPolicyAllowedDevicesTest, InitializeWithExistingEmptyPrefValue) {
  SetAllowDevicesForUrlsPrefValue("[]");
  SetAllowDevicesForUrlsOnLoginScreenPrefValue("[]");
  SetAllowDevicesWithHidUsagesForUrlsPrefValue("[]");
  SetAllowAllDevicesForUrlsPrefValue("[]");

  InitializePolicy();

  EXPECT_EQ(0u, policy()->all_devices_policy().size());
  EXPECT_EQ(0u, policy()->vendor_policy().size());
  EXPECT_EQ(0u, policy()->device_policy().size());
  EXPECT_EQ(0u, policy()->usage_page_policy().size());
  EXPECT_EQ(0u, policy()->usage_policy().size());
}

namespace {

constexpr uint16_t kTestVendorId1 = 1234;
constexpr uint16_t kTestVendorId2 = 4321;
constexpr uint16_t kTestProductId1 = 5678;
constexpr uint16_t kTestProductId2 = 8765;
constexpr uint16_t kTestUsagePage1 = 1;
constexpr uint16_t kTestUsagePage2 = 2;
constexpr uint16_t kTestUsage1 = 4;
constexpr uint16_t kTestUsage2 = 5;

constexpr char kAllowDevicesForUrls[] = R"(
      [
        {
          "devices": [
            { "vendor_id": 1234, "product_id": 5678 },
            { "vendor_id": 4321 }
          ],
          "urls": [ "https://origin1" ]
        }
      ])";
constexpr char kAllowDevicesWithHidUsagesForUrls[] = R"(
      [
        {
          "usages": [
            { "usage_page": 1, "usage": 4 },
            { "usage_page": 2 }
          ],
          "urls": [ "https://origin2" ]
        }
      ])";
constexpr char kAllowAllDevicesForUrls[] = R"([ "https://origin3" ])";

}  // namespace

TEST_F(HidPolicyAllowedDevicesTest, InitializeWithPrefValues) {
  const auto kOrigin1 = url::Origin::Create(GURL("https://origin1"));
  const auto kOrigin2 = url::Origin::Create(GURL("https://origin2"));
  const auto kOrigin3 = url::Origin::Create(GURL("https://origin3"));

  SetAllowDevicesForUrlsPrefValue(kAllowDevicesForUrls);
  SetAllowDevicesWithHidUsagesForUrlsPrefValue(
      kAllowDevicesWithHidUsagesForUrls);
  SetAllowAllDevicesForUrlsPrefValue(kAllowAllDevicesForUrls);
  InitializePolicy();

  EXPECT_EQ(1u, policy()->device_policy().size());
  EXPECT_EQ(1u, policy()->vendor_policy().size());
  EXPECT_EQ(1u, policy()->usage_page_policy().size());
  EXPECT_EQ(1u, policy()->usage_policy().size());
  EXPECT_EQ(1u, policy()->all_devices_policy().size());

  const auto device_key = std::make_pair(kTestVendorId1, kTestProductId1);
  ASSERT_TRUE(base::Contains(policy()->device_policy(), device_key));
  EXPECT_THAT(policy()->device_policy().at(device_key),
              UnorderedElementsAre(kOrigin1));

  ASSERT_TRUE(base::Contains(policy()->vendor_policy(), kTestVendorId2));
  EXPECT_THAT(policy()->vendor_policy().at(kTestVendorId2),
              UnorderedElementsAre(kOrigin1));

  const auto usage_key = std::make_pair(kTestUsagePage1, kTestUsage1);
  ASSERT_TRUE(base::Contains(policy()->usage_policy(), usage_key));
  EXPECT_THAT(policy()->usage_policy().at(usage_key),
              UnorderedElementsAre(kOrigin2));

  ASSERT_TRUE(base::Contains(policy()->usage_page_policy(), kTestUsagePage2));
  EXPECT_THAT(policy()->usage_page_policy().at(kTestUsagePage2),
              UnorderedElementsAre(kOrigin2));

  EXPECT_THAT(policy()->all_devices_policy(), UnorderedElementsAre(kOrigin3));

  auto device = CreateAndAddDevice(kTestVendorId1, kTestProductId1,
                                   /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));

  device = CreateAndAddDevice(kTestVendorId1, kTestProductId2,
                              /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));

  device = CreateAndAddDevice(kTestVendorId2, kTestProductId2,
                              /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              kTestUsagePage1, kTestUsage1);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              kTestUsagePage1, kTestUsage2);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              kTestUsagePage2, kTestUsage2);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));
}

TEST_F(HidPolicyAllowedDevicesTest, InitializeWithMissingPrefValuesThenUpdate) {
  const auto kOrigin1 = url::Origin::Create(GURL("https://origin1"));
  const auto kOrigin2 = url::Origin::Create(GURL("https://origin2"));
  const auto kOrigin3 = url::Origin::Create(GURL("https://origin3"));

  InitializePolicy();

  EXPECT_EQ(0u, policy()->device_policy().size());
  EXPECT_EQ(0u, policy()->vendor_policy().size());
  EXPECT_EQ(0u, policy()->usage_page_policy().size());
  EXPECT_EQ(0u, policy()->usage_policy().size());
  EXPECT_EQ(0u, policy()->all_devices_policy().size());

  SetAllowDevicesForUrlsPrefValue(kAllowDevicesForUrls);
  SetAllowDevicesWithHidUsagesForUrlsPrefValue(
      kAllowDevicesWithHidUsagesForUrls);
  SetAllowAllDevicesForUrlsPrefValue(kAllowAllDevicesForUrls);

  EXPECT_EQ(1u, policy()->device_policy().size());
  EXPECT_EQ(1u, policy()->vendor_policy().size());
  EXPECT_EQ(1u, policy()->usage_page_policy().size());
  EXPECT_EQ(1u, policy()->usage_policy().size());
  EXPECT_EQ(1u, policy()->all_devices_policy().size());

  const auto device_key = std::make_pair(kTestVendorId1, kTestProductId1);
  ASSERT_TRUE(base::Contains(policy()->device_policy(), device_key));
  EXPECT_THAT(policy()->device_policy().at(device_key),
              UnorderedElementsAre(kOrigin1));

  ASSERT_TRUE(base::Contains(policy()->vendor_policy(), kTestVendorId2));
  EXPECT_THAT(policy()->vendor_policy().at(kTestVendorId2),
              UnorderedElementsAre(kOrigin1));

  const auto usage_key = std::make_pair(kTestUsagePage1, kTestUsage1);
  ASSERT_TRUE(base::Contains(policy()->usage_policy(), usage_key));
  EXPECT_THAT(policy()->usage_policy().at(usage_key),
              UnorderedElementsAre(kOrigin2));

  ASSERT_TRUE(base::Contains(policy()->usage_page_policy(), kTestUsagePage2));
  EXPECT_THAT(policy()->usage_page_policy().at(kTestUsagePage2),
              UnorderedElementsAre(kOrigin2));

  EXPECT_THAT(policy()->all_devices_policy(), UnorderedElementsAre(kOrigin3));

  auto device = CreateAndAddDevice(kTestVendorId1, kTestProductId1,
                                   /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));

  device = CreateAndAddDevice(kTestVendorId1, kTestProductId2,
                              /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));

  device = CreateAndAddDevice(kTestVendorId2, kTestProductId2,
                              /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              kTestUsagePage1, kTestUsage1);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              kTestUsagePage1, kTestUsage2);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              kTestUsagePage2, kTestUsage2);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));
}

TEST_F(HidPolicyAllowedDevicesTest,
       InitializeWithMissingPrefValuesThenUpdateOnLoginScreen) {
  const auto kOrigin1 = url::Origin::Create(GURL("https://origin1"));
  const auto kOrigin2 = url::Origin::Create(GURL("https://origin2"));

  InitializePolicy(/*on_login_screen=*/true);

  EXPECT_EQ(0u, policy()->device_policy().size());
  EXPECT_EQ(0u, policy()->vendor_policy().size());
  EXPECT_EQ(0u, policy()->usage_page_policy().size());
  EXPECT_EQ(0u, policy()->usage_policy().size());
  EXPECT_EQ(0u, policy()->all_devices_policy().size());

  SetAllowDevicesForUrlsOnLoginScreenPrefValue(kAllowDevicesForUrls);

  EXPECT_EQ(1u, policy()->device_policy().size());
  EXPECT_EQ(1u, policy()->vendor_policy().size());
  EXPECT_EQ(0u, policy()->usage_page_policy().size());
  EXPECT_EQ(0u, policy()->usage_policy().size());
  EXPECT_EQ(0u, policy()->all_devices_policy().size());

  const auto device_key = std::make_pair(kTestVendorId1, kTestProductId1);
  ASSERT_TRUE(base::Contains(policy()->device_policy(), device_key));
  EXPECT_THAT(policy()->device_policy().at(device_key),
              UnorderedElementsAre(kOrigin1));

  ASSERT_TRUE(base::Contains(policy()->vendor_policy(), kTestVendorId2));
  EXPECT_THAT(policy()->vendor_policy().at(kTestVendorId2),
              UnorderedElementsAre(kOrigin1));

  auto device = CreateAndAddDevice(kTestVendorId1, kTestProductId1,
                                   /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));

  device = CreateAndAddDevice(kTestVendorId1, kTestProductId2,
                              /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));

  device = CreateAndAddDevice(kTestVendorId2, kTestProductId2,
                              /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
}

TEST_F(HidPolicyAllowedDevicesTest, InitializeWithPrefValuesThenRemovePolicy) {
  SetAllowDevicesForUrlsPrefValue(kAllowDevicesForUrls);
  SetAllowDevicesWithHidUsagesForUrlsPrefValue(
      kAllowDevicesWithHidUsagesForUrls);
  SetAllowAllDevicesForUrlsPrefValue(kAllowAllDevicesForUrls);
  InitializePolicy();

  EXPECT_EQ(1u, policy()->device_policy().size());
  EXPECT_EQ(1u, policy()->vendor_policy().size());
  EXPECT_EQ(1u, policy()->usage_page_policy().size());
  EXPECT_EQ(1u, policy()->usage_policy().size());
  EXPECT_EQ(1u, policy()->all_devices_policy().size());

  // Change each preference back to the default value.
  SetAllowDevicesForUrlsPrefValue("[]");
  SetAllowDevicesWithHidUsagesForUrlsPrefValue("[]");
  SetAllowAllDevicesForUrlsPrefValue("[]");

  EXPECT_EQ(0u, policy()->device_policy().size());
  EXPECT_EQ(0u, policy()->vendor_policy().size());
  EXPECT_EQ(0u, policy()->usage_page_policy().size());
  EXPECT_EQ(0u, policy()->usage_policy().size());
  EXPECT_EQ(0u, policy()->all_devices_policy().size());
}

TEST_F(HidPolicyAllowedDevicesTest, MultipleUrls) {
  const auto kOrigin1 = url::Origin::Create(GURL("https://origin1"));
  const auto kOrigin2 = url::Origin::Create(GURL("https://origin2"));

  SetAllowDevicesForUrlsPrefValue(R"(
      [
        {
          "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
          "urls": [
            "https://origin1",
            "https://origin2"
          ]
        },
        {
          "devices": [{ "vendor_id": 1234 }],
          "urls": [
            "https://origin1",
            "https://origin2"
          ]
        }
      ])");
  SetAllowDevicesWithHidUsagesForUrlsPrefValue(R"(
      [
        {
          "usages": [{ "usage_page": 1, "usage": 4 }],
          "urls": [
            "https://origin1",
            "https://origin2"
          ]
        },
        {
          "usages": [{ "usage_page": 1 }],
          "urls": [
            "https://origin1",
            "https://origin2"
          ]
        }
      ])");
  SetAllowAllDevicesForUrlsPrefValue(R"(
      [
        "https://origin1",
        "https://origin2"
      ])");
  InitializePolicy();

  EXPECT_EQ(1u, policy()->device_policy().size());
  EXPECT_EQ(1u, policy()->vendor_policy().size());
  EXPECT_EQ(1u, policy()->usage_page_policy().size());
  EXPECT_EQ(1u, policy()->usage_policy().size());
  EXPECT_EQ(2u, policy()->all_devices_policy().size());

  const auto device_key = std::make_pair(kTestVendorId1, kTestProductId1);
  ASSERT_TRUE(base::Contains(policy()->device_policy(), device_key));
  EXPECT_THAT(policy()->device_policy().at(device_key),
              UnorderedElementsAre(kOrigin1, kOrigin2));

  ASSERT_TRUE(base::Contains(policy()->vendor_policy(), kTestVendorId1));
  EXPECT_THAT(policy()->vendor_policy().at(kTestVendorId1),
              UnorderedElementsAre(kOrigin1, kOrigin2));

  const auto usage_key = std::make_pair(kTestUsagePage1, kTestUsage1);
  ASSERT_TRUE(base::Contains(policy()->usage_policy(), usage_key));
  EXPECT_THAT(policy()->usage_policy().at(usage_key),
              UnorderedElementsAre(kOrigin1, kOrigin2));

  ASSERT_TRUE(base::Contains(policy()->usage_page_policy(), kTestUsagePage1));
  EXPECT_THAT(policy()->usage_page_policy().at(kTestUsagePage1),
              UnorderedElementsAre(kOrigin1, kOrigin2));

  EXPECT_THAT(policy()->all_devices_policy(),
              UnorderedElementsAre(kOrigin1, kOrigin2));

  auto device = CreateAndAddDevice(kTestVendorId1, kTestProductId1,
                                   kTestUsagePage1, kTestUsage1);
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin2, *device));
}

TEST_F(HidPolicyAllowedDevicesTest, MultipleItemsWithOverlap) {
  const auto kOrigin1 = url::Origin::Create(GURL("https://origin1"));
  const auto kOrigin2 = url::Origin::Create(GURL("https://origin2"));
  const auto kOrigin3 = url::Origin::Create(GURL("https://origin3"));
  const auto kOrigin4 = url::Origin::Create(GURL("https://origin4"));

  SetAllowDevicesForUrlsPrefValue(R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 },
          { "vendor_id": 4321 }
        ],
        "urls": [ "https://origin1" ]
      },
      {
        "devices": [
          { "vendor_id": 1234 },
          { "vendor_id": 4321, "product_id": 8765 }
        ],
        "urls": [ "https://origin2" ]
      }
    ])");
  SetAllowDevicesWithHidUsagesForUrlsPrefValue(R"(
    [
      {
        "usages": [
          { "usage_page": 1, "usage": 4 },
          { "usage_page": 2 }
        ],
        "urls": [ "https://origin3" ]
      },
      {
        "usages": [
          { "usage_page": 1 },
          { "usage_page": 2, "usage": 5 }
        ],
        "urls": [ "https://origin4" ]
      }
    ])");
  InitializePolicy();

  EXPECT_EQ(2u, policy()->device_policy().size());
  EXPECT_EQ(2u, policy()->vendor_policy().size());
  EXPECT_EQ(2u, policy()->usage_policy().size());
  EXPECT_EQ(2u, policy()->usage_page_policy().size());
  EXPECT_EQ(0u, policy()->all_devices_policy().size());

  const auto device_key1 = std::make_pair(kTestVendorId1, kTestProductId1);
  ASSERT_TRUE(base::Contains(policy()->device_policy(), device_key1));
  EXPECT_THAT(policy()->device_policy().at(device_key1),
              UnorderedElementsAre(kOrigin1));

  const auto device_key2 = std::make_pair(kTestVendorId2, kTestProductId2);
  ASSERT_TRUE(base::Contains(policy()->device_policy(), device_key2));
  EXPECT_THAT(policy()->device_policy().at(device_key2),
              UnorderedElementsAre(kOrigin2));

  ASSERT_TRUE(base::Contains(policy()->vendor_policy(), kTestVendorId1));
  EXPECT_THAT(policy()->vendor_policy().at(kTestVendorId1),
              UnorderedElementsAre(kOrigin2));

  ASSERT_TRUE(base::Contains(policy()->vendor_policy(), kTestVendorId2));
  EXPECT_THAT(policy()->vendor_policy().at(kTestVendorId2),
              UnorderedElementsAre(kOrigin1));

  const auto usage_key1 = std::make_pair(kTestUsagePage1, kTestUsage1);
  ASSERT_TRUE(base::Contains(policy()->usage_policy(), usage_key1));
  EXPECT_THAT(policy()->usage_policy().at(usage_key1),
              UnorderedElementsAre(kOrigin3));

  const auto usage_key2 = std::make_pair(kTestUsagePage2, kTestUsage2);
  ASSERT_TRUE(base::Contains(policy()->usage_policy(), usage_key2));
  EXPECT_THAT(policy()->usage_policy().at(usage_key2),
              UnorderedElementsAre(kOrigin4));

  ASSERT_TRUE(base::Contains(policy()->usage_page_policy(), kTestUsagePage1));
  EXPECT_THAT(policy()->usage_page_policy().at(kTestUsagePage1),
              UnorderedElementsAre(kOrigin4));

  ASSERT_TRUE(base::Contains(policy()->usage_page_policy(), kTestUsagePage2));
  EXPECT_THAT(policy()->usage_page_policy().at(kTestUsagePage2),
              UnorderedElementsAre(kOrigin3));

  auto device = CreateAndAddDevice(kTestVendorId1, kTestProductId1,
                                   /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin3, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin4, *device));

  device = CreateAndAddDevice(kTestVendorId1, kTestProductId2,
                              /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin3, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin4, *device));

  device = CreateAndAddDevice(kTestVendorId2, kTestProductId2,
                              /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin3, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin4, *device));

  device = CreateAndAddDevice(kTestVendorId2, kTestProductId1,
                              /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin3, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin4, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              kTestUsagePage1, kTestUsage1);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin4, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              kTestUsagePage1, kTestUsage2);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin3, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin4, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              kTestUsagePage2, kTestUsage2);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin4, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              kTestUsagePage2, kTestUsage1);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_TRUE(policy()->HasDevicePermission(kOrigin3, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin4, *device));

  device = CreateAndAddDevice(/*vendor_id=*/1, /*product_id=*/1,
                              /*usage_page=*/0xff00, /*usage=*/1);
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin1, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin2, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin3, *device));
  EXPECT_FALSE(policy()->HasDevicePermission(kOrigin4, *device));
}
