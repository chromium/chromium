// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

const char kDevicesKey[] = "devices";
const char kUrlPatternsKey[] = "url_patterns";
const char kVendorIdKey[] = "vendor_id";
const char kProductIdKey[] = "product_id";
const char kValidPolicy[] =
    "["
    "  {"
    "    \"devices\": ["
    // Ensure that a device can have both IDs.
    "      {"
    "        \"vendor_id\": 1234,"
    "        \"product_id\": 5678"
    // Ensure that a device can have only a
    // |vendor_id|.
    "      }, {"
    "        \"vendor_id\": 4321"
    "      }"
    "    ],"
    "    \"url_patterns\": ["
    "      \"[*.]google.com\","
    "      \"youtube.com\""
    "    ]"
    "  }, {"
    // Ensure that a device can have neither IDs.
    "    \"devices\": [{ }],"
    "    \"url_patterns\": [\"[*.]crbug.com\"]"
    "  }"
    "]";
// An invalid entry invalidates the entire policy.
const char kInvalidPolicyInvalidTopLevelEntry[] =
    "["
    "  {"
    "    \"devices\": ["
    "      {"
    "        \"vendor_id\": 1234,"
    "        \"product_id\": 5678"
    "      }, {"
    "        \"vendor_id\": 4321"
    "      }"
    "    ],"
    "    \"url_patterns\": ["
    "      \"[*.]google.com\","
    "      \"youtube.com\""
    "    ]"
    "  }, {"
    "    \"url_patterns\": [\"[*.]crbug.com\"]"
    "  }"
    "]";
// A list item must have both |devices| and
// |url_patterns| specified.
const char kInvalidPolicyMissingDevicesProperty[] =
    "["
    "  {"
    "    \"url_patterns\": ["
    "      \"[*.]google.com\","
    "      \"youtube.com\""
    "    ]"
    "  }"
    "]";
const char kInvalidPolicyMissingUrlPatternsProperty[] =
    "["
    "  {"
    "    \"devices\": ["
    "      {"
    "        \"vendor_id\": 1234,"
    "        \"product_id\": 5678"
    "      }"
    "    ]"
    "  }"
    "]";
// The |vendor_id| and |product_id| values should fit into an unsigned short.
const char kInvalidPolicyMismatchedVendorIdType[] =
    "["
    "  {"
    "    \"devices\": ["
    "      {"
    "        \"vendor_id\": 70000,"
    "        \"product_id\": 5678"
    "      }"
    "    ],"
    "    \"url_patterns\": ["
    "      \"[*.]google.com\","
    "      \"youtube.com\""
    "    ]"
    "  }"
    "]";
const char kInvalidPolicyMismatchedProductIdType[] =
    "["
    "  {"
    "    \"devices\": ["
    "      {"
    "        \"vendor_id\": 1234,"
    "        \"product_id\": 70000"
    "      }"
    "    ],"
    "    \"url_patterns\": ["
    "      \"[*.]google.com\","
    "      \"youtube.com\""
    "    ]"
    "  }"
    "]";
// Unknown properties invalidate the policy.
const char kInvalidPolicyUnknownProperty[] =
    "["
    "  {"
    "    \"devices\": ["
    "      {"
    "        \"vendor_id\": 1234,"
    "        \"product_id\": 5678,"
    "        \"serialNumber\": \"1234ABCD\""
    "      }"
    "    ],"
    "    \"url_patterns\": ["
    "      \"[*.]google.com\","
    "      \"youtube.com\""
    "    ]"
    "  }"
    "]";
// A device containing a |product_id| must also have a |vendor_id|.
const char kInvalidPolicyProductIdWithoutVendorId[] =
    "["
    "  {"
    "    \"devices\": ["
    "      {"
    "        \"product_id\": 5678"
    "      }"
    "    ],"
    "    \"url_patterns\": ["
    "      \"[*.]google.com\","
    "      \"youtube.com\""
    "    ]"
    "  }"
    "]";

}  // namespace

class WebUsbAllowDevicesForUrlsPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  WebUsbAllowDevicesForUrlsPolicyHandler* handler() { return handler_; }

 private:
  void SetUp() override {
    Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
    auto handler =
        std::make_unique<WebUsbAllowDevicesForUrlsPolicyHandler>(chrome_schema);
    handler_ = handler.get();
    handler_list_.AddHandler(std::move(handler));
  }

  WebUsbAllowDevicesForUrlsPolicyHandler* handler_;
};

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest, CheckPolicySettings) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kValidPolicy), nullptr);
  ASSERT_TRUE(errors.empty());
  EXPECT_TRUE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidTopLevelEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyInvalidTopLevelEntry), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  ASSERT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[1]\": Missing or invalid required "
      "property: devices");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMissingDevicesProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyMissingDevicesProperty), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  ASSERT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0]\": Missing or invalid required "
      "property: devices");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMissingUrlPatternsProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyMissingUrlPatternsProperty),
      nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  ASSERT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0]\": Missing or invalid required "
      "property: url_patterns");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsUnknownProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyUnknownProperty), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  ASSERT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": Unknown "
      "property: serialNumber");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMismatchedVendorIdType) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyMismatchedVendorIdType), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  ASSERT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": The vendor_id "
      "must be an unsigned short integer");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMismatchedProductIdType) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyMismatchedProductIdType), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  ASSERT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": The "
      "product_id must be an unsigned short integer");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithProductIdWithoutVendorId) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyProductIdWithoutVendorId), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  ASSERT_EQ(errors.size(), 1ul);

  const base::string16 kExpected = base::ASCIIToUTF16(
      "Schema validation error at \"items[0].devices.items[0]\": A vendor_id "
      "must also be specified");
  EXPECT_EQ(errors.GetErrors(key::kWebUsbAllowDevicesForUrls), kExpected);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest, ApplyPolicySettings) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kValidPolicy), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  ASSERT_TRUE(pref_value);
  ASSERT_TRUE(pref_value->is_list());

  // Ensure that the kManagedWebUsbAllowDevicesForUrls pref is set correctly.
  const base::Value::ListStorage& list = pref_value->GetList();
  ASSERT_EQ(list.size(), 2ul);

  // Check the first item's devices list.
  const base::Value* devices = list[0].FindKey(kDevicesKey);
  ASSERT_TRUE(devices);

  const base::Value::ListStorage& first_devices_list = devices->GetList();
  ASSERT_EQ(first_devices_list.size(), 2ul);

  const base::Value* vendor_id = first_devices_list[0].FindKey(kVendorIdKey);
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(vendor_id->GetInt(), 1234);

  const base::Value* product_id = first_devices_list[0].FindKey(kProductIdKey);
  ASSERT_TRUE(product_id);
  EXPECT_EQ(product_id->GetInt(), 5678);

  vendor_id = first_devices_list[1].FindKey(kVendorIdKey);
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(vendor_id->GetInt(), 4321);

  product_id = first_devices_list[1].FindKey(kProductIdKey);
  EXPECT_FALSE(product_id);

  // Check the first item's url_patterns list.
  const base::Value* url_patterns = list[0].FindKey(kUrlPatternsKey);
  ASSERT_TRUE(url_patterns);

  const base::Value::ListStorage& first_url_patterns_list =
      url_patterns->GetList();
  ASSERT_EQ(first_url_patterns_list.size(), 2ul);
  ASSERT_TRUE(first_url_patterns_list[0].is_string());
  ASSERT_TRUE(first_url_patterns_list[1].is_string());
  EXPECT_EQ(first_url_patterns_list[0].GetString(), "[*.]google.com");
  EXPECT_EQ(first_url_patterns_list[1].GetString(), "youtube.com");

  // Check the second item's devices list.
  devices = list[1].FindKey(kDevicesKey);
  ASSERT_TRUE(devices);

  const base::Value::ListStorage& second_devices_list = devices->GetList();
  ASSERT_EQ(second_devices_list.size(), 1ul);

  vendor_id = second_devices_list[0].FindKey(kVendorIdKey);
  EXPECT_FALSE(vendor_id);

  product_id = second_devices_list[0].FindKey(kProductIdKey);
  EXPECT_FALSE(product_id);

  // Check the second item's url_patterns list.
  url_patterns = list[1].FindKey(kUrlPatternsKey);
  ASSERT_TRUE(url_patterns);

  const base::Value::ListStorage& second_url_patterns_list =
      url_patterns->GetList();
  ASSERT_EQ(second_url_patterns_list.size(), 1ul);
  ASSERT_TRUE(second_url_patterns_list[0].is_string());
  EXPECT_EQ(second_url_patterns_list[0].GetString(), "[*.]crbug.com");
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithInvalidTopLevelEntry) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyInvalidTopLevelEntry), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMissingDevicesProperty) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyMissingDevicesProperty), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMissingUrlPatternsProperty) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyMissingUrlPatternsProperty),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithUnknownProperty) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyUnknownProperty), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMismatchedVendorIdType) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyMismatchedVendorIdType), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMismatchedProductIdType) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyMismatchedProductIdType), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsProductIdWithoutVendorId) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kInvalidPolicyProductIdWithoutVendorId), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

}  // namespace policy
