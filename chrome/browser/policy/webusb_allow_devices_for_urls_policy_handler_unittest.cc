// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// For ChromeOS this test is parameterized to test user and device policy. For
// other operating systems, this test just tests the user policy.
enum class PolicyType { kUser, kDevice };

constexpr char kDevicesKey[] = "devices";
constexpr char kUrlsKey[] = "urls";
constexpr char kVendorIdKey[] = "vendor_id";
constexpr char kProductIdKey[] = "product_id";
// This policy contains several valid entries. A valid |devices| item is an
// object that contains both IDs, only the |vendor_id|, or neither IDs. A valid
// |urls| entry is a string containing up to two valid URLs delimited by a
// comma.
constexpr char kValidPolicy[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          }, {
            "vendor_id": 4321
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }, {
        "devices": [{ }],
        "urls": ["https://chromium.org,"]
      }
    ])";
// An invalid entry invalidates the entire policy.
constexpr char kInvalidPolicyInvalidTopLevelEntry[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          }, {
            "vendor_id": 4321
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }, {
        "urls": ["https://crbug.com"]
      }
    ])";
// A list item must have both |devices| and |urls| specified.
constexpr char kInvalidPolicyMissingDevicesProperty[] = R"(
    [
      {
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
constexpr char kInvalidPolicyMissingUrlsProperty[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          }
        ]
      }
    ])";
// The |vendor_id| and |product_id| values should fit into an unsigned short.
constexpr char kInvalidPolicyMismatchedVendorIdType[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 70000,
            "product_id": 5678
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
constexpr char kInvalidPolicyMismatchedProductIdType[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 70000
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
// Unknown properties invalidate the policy.
constexpr char kInvalidPolicyUnknownProperty[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678,
            "serialNumber": "1234ABCD"
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
// Same as |kInvalidPolicyUnknownProperty| without the unknown property
// "serialNumber". This serves as expected pref value of applying the policy
// with |kInvalidPolicyUnknownProperty|.
constexpr char kInvalidPolicyUnknownPropertyAfterCleanup[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
// A device containing a |product_id| must also have a |vendor_id|.
constexpr char kInvalidPolicyProductIdWithoutVendorId[] = R"(
    [
      {
        "devices": [
          {
            "product_id": 5678
          }
        ],
        "urls": [
          "https://google.com,https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";
// The |urls| array must contain valid URLs.
constexpr char kInvalidPolicyInvalidRequestingUrl[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["some.invalid.url"]
      }
    ])";
constexpr char kInvalidPolicyInvalidEmbeddingUrl[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["https://google.com,some.invalid.url"]
      }
    ])";
constexpr char kInvalidPolicyInvalidUrlsEntry[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["https://google.com,https://google.com,https://google.com"]
      }
    ])";
constexpr char InvalidPolicyNoUrls[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": [""]
      }
    ])";

std::string GetPolicyName(PolicyType policy_type) {
#if defined(OS_CHROMEOS)
  if (policy_type == PolicyType::kDevice)
    return key::kDeviceLoginScreenWebUsbAllowDevicesForUrls;
#endif  // defined(OS_CHROMEOS)
  return key::kWebUsbAllowDevicesForUrls;
}

std::string GetPrefName(PolicyType policy_type) {
#if defined(OS_CHROMEOS)
  if (policy_type == PolicyType::kDevice)
    return prefs::kDeviceLoginScreenWebUsbAllowDevicesForUrls;
#endif  // defined(OS_CHROMEOS)
  return prefs::kManagedWebUsbAllowDevicesForUrls;
}

std::unique_ptr<WebUsbAllowDevicesForUrlsPolicyHandler> CreateHandler(
    PolicyType policy_type,
    const Schema& chrome_schema) {
#if defined(OS_CHROMEOS)
  if (policy_type == PolicyType::kDevice) {
    return WebUsbAllowDevicesForUrlsPolicyHandler::CreateForDevicePolicy(
        chrome_schema);
  }
#endif  // defined(OS_CHROMEOS)
  return WebUsbAllowDevicesForUrlsPolicyHandler::CreateForUserPolicy(
      chrome_schema);
}

std::unique_ptr<base::Value> ReadJson(base::StringPiece json) {
  base::Optional<base::Value> value = base::JSONReader::Read(json);
  EXPECT_TRUE(value);
  return value ? base::Value::ToUniquePtrValue(std::move(*value)) : nullptr;
}

}  // namespace

class WebUsbAllowDevicesForUrlsPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest,
      public testing::WithParamInterface<PolicyType> {
 public:
  WebUsbAllowDevicesForUrlsPolicyHandlerTest()
      : policy_name_(GetPolicyName(GetParam())),
        pref_name_(GetPrefName(GetParam())) {}

  WebUsbAllowDevicesForUrlsPolicyHandler* handler() { return handler_; }

 protected:
  const std::string policy_name_;
  const std::string pref_name_;

 private:
  void SetUp() override {
    Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
    auto handler = CreateHandler(GetParam(), chrome_schema);
    handler_ = handler.get();
    handler_list_.AddHandler(std::move(handler));
  }

  WebUsbAllowDevicesForUrlsPolicyHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(WebUsbAllowDevicesForUrlsPolicyHandlerTest);
};

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest, CheckPolicySettings) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kValidPolicy),
             nullptr);
  ASSERT_TRUE(errors.empty());
  EXPECT_TRUE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidTopLevelEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyInvalidTopLevelEntry), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[1]\": Missing or invalid required "
      "property: devices");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMissingDevicesProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyMissingDevicesProperty), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0]\": Missing or invalid required "
      "property: devices");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMissingUrlsProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyMissingUrlsProperty), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0]\": Missing or invalid required "
      "property: urls");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsUnknownProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyUnknownProperty), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_TRUE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": Unknown "
      "property: serialNumber");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMismatchedVendorIdType) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyMismatchedVendorIdType), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": The "
      "vendor_id must be an unsigned short integer");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMismatchedProductIdType) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyMismatchedProductIdType), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": The "
      "product_id must be an unsigned short integer");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithProductIdWithoutVendorId) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyProductIdWithoutVendorId), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": A vendor_id "
      "must also be specified");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidRequestingUrlEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyInvalidRequestingUrl), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].urls.items[0]\": The urls item "
      "must contain valid URLs");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidEmbeddingUrlEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyInvalidEmbeddingUrl), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].urls.items[0]\": The urls item "
      "must contain valid URLs");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidUrlsEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyInvalidUrlsEntry), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].urls.items[0]\": Each urls "
      "string entry must contain between 1 to 2 URLs");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithNoUrls) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(InvalidPolicyNoUrls),
             nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].urls.items[0]\": Each urls "
      "string entry must contain between 1 to 2 URLs");
  EXPECT_EQ(kExpected, errors.GetErrors(policy_name_));
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest, ApplyPolicySettings) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kValidPolicy),
             nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(store_->GetValue(pref_name_, &pref_value));
  ASSERT_TRUE(pref_value);
  ASSERT_TRUE(pref_value->is_list());

  // Ensure that the kManagedWebUsbAllowDevicesForUrls pref is set correctly.
  const auto& list = pref_value->GetList();
  ASSERT_EQ(2ul, list.size());

  // Check the first item's devices list.
  const base::Value* devices = list[0].FindKey(kDevicesKey);
  ASSERT_TRUE(devices);

  const auto& first_devices_list = devices->GetList();
  ASSERT_EQ(2ul, first_devices_list.size());

  const base::Value* vendor_id = first_devices_list[0].FindKey(kVendorIdKey);
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(1234, vendor_id->GetInt());

  const base::Value* product_id = first_devices_list[0].FindKey(kProductIdKey);
  ASSERT_TRUE(product_id);
  EXPECT_EQ(5678, product_id->GetInt());

  vendor_id = first_devices_list[1].FindKey(kVendorIdKey);
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(4321, vendor_id->GetInt());

  product_id = first_devices_list[1].FindKey(kProductIdKey);
  EXPECT_FALSE(product_id);

  // Check the first item's urls list.
  const base::Value* urls = list[0].FindKey(kUrlsKey);
  ASSERT_TRUE(urls);

  const auto& first_urls_list = urls->GetList();
  ASSERT_EQ(2ul, first_urls_list.size());
  ASSERT_TRUE(first_urls_list[0].is_string());
  ASSERT_TRUE(first_urls_list[1].is_string());
  EXPECT_EQ("https://google.com,https://google.com",
            first_urls_list[0].GetString());
  EXPECT_EQ("https://www.youtube.com", first_urls_list[1].GetString());

  // Check the second item's devices list.
  devices = list[1].FindKey(kDevicesKey);
  ASSERT_TRUE(devices);

  const auto& second_devices_list = devices->GetList();
  ASSERT_EQ(1ul, second_devices_list.size());

  vendor_id = second_devices_list[0].FindKey(kVendorIdKey);
  EXPECT_FALSE(vendor_id);

  product_id = second_devices_list[0].FindKey(kProductIdKey);
  EXPECT_FALSE(product_id);

  // Check the second item's urls list.
  urls = list[1].FindKey(kUrlsKey);
  ASSERT_TRUE(urls);

  const auto& second_urls_list = urls->GetList();
  ASSERT_EQ(1ul, second_urls_list.size());
  ASSERT_TRUE(second_urls_list[0].is_string());
  EXPECT_EQ("https://chromium.org,", second_urls_list[0].GetString());
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithInvalidTopLevelEntry) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyInvalidTopLevelEntry), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMissingDevicesProperty) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyMissingDevicesProperty), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMissingUrlsProperty) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyMissingUrlsProperty), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithUnknownProperty) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyUnknownProperty), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_TRUE(pref_value);

  std::unique_ptr<base::Value> expected_pref_value =
      ReadJson(kInvalidPolicyUnknownPropertyAfterCleanup);
  EXPECT_EQ(*expected_pref_value, *pref_value);
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMismatchedVendorIdType) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyMismatchedVendorIdType), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMismatchedProductIdType) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyMismatchedProductIdType), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsProductIdWithoutVendorId) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyProductIdWithoutVendorId), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsInvalidRequestingUrl) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyInvalidRequestingUrl), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsInvalidEmbeddingUrl) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyInvalidEmbeddingUrl), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsInvalidUrlsEntry) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ReadJson(kInvalidPolicyInvalidUrlsEntry), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_P(WebUsbAllowDevicesForUrlsPolicyHandlerTest, ApplyPolicySettingsNoUrls) {
  EXPECT_FALSE(store_->GetValue(pref_name_, nullptr));

  PolicyMap policy;
  policy.Set(policy_name_, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(InvalidPolicyNoUrls),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(pref_name_, &pref_value));
  EXPECT_FALSE(pref_value);
}

// For ChromeOS this test is parameterized to test user and device policy. For
// other operating systems, this test just tests the user policy.
INSTANTIATE_TEST_SUITE_P(,
                         WebUsbAllowDevicesForUrlsPolicyHandlerTest,
#if defined(OS_CHROMEOS)
                         testing::Values(PolicyType::kUser, PolicyType::kDevice)
#else
                         testing::Values(PolicyType::kUser)
#endif
);

}  // namespace policy
