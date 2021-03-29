// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

base::Optional<base::Value> ReadJson(base::StringPiece json) {
  auto result = base::JSONReader::ReadAndReturnValueWithError(json);
  EXPECT_TRUE(result.value) << result.error_message;
  return std::move(result.value);
}

}  // namespace

// There is no real SerialAllowUsbDevicesForUrlsPolicyHandler class as this
// policy uses SimpleSchemaValidatingPolicyHandler but having dedicated test
// cases is useful to ensure expected behavior.
class SerialAllowUsbDevicesForUrlsPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  SerialAllowUsbDevicesForUrlsPolicyHandlerTest() = default;
  SerialAllowUsbDevicesForUrlsPolicyHandlerTest(
      SerialAllowUsbDevicesForUrlsPolicyHandlerTest& other) = delete;
  SerialAllowUsbDevicesForUrlsPolicyHandlerTest& operator=(
      SerialAllowUsbDevicesForUrlsPolicyHandlerTest& other) = delete;
  ~SerialAllowUsbDevicesForUrlsPolicyHandlerTest() override = default;

  ConfigurationPolicyHandler* handler() { return handler_; }

 private:
  void SetUp() override {
    Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
    auto handler = std::make_unique<SimpleSchemaValidatingPolicyHandler>(
        key::kSerialAllowUsbDevicesForUrls,
        prefs::kManagedSerialAllowUsbDevicesForUrls, chrome_schema,
        SCHEMA_ALLOW_UNKNOWN,
        SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
        SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED);
    handler_ = handler.get();
    handler_list_.AddHandler(std::move(handler));
  }

  ConfigurationPolicyHandler* handler_;
};

TEST_F(SerialAllowUsbDevicesForUrlsPolicyHandlerTest, CheckPolicySettings) {
  constexpr char kPolicy[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          },
          {"vendor_id": 4321}
        ],
        "urls": [
          "https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";

  PolicyMap policy;
  policy.Set(key::kSerialAllowUsbDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_TRUE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.empty());

  // Now try to apply the policy and validate it has been stored in preferences.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls, nullptr));

  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls,
                               &pref_value));
  EXPECT_TRUE(pref_value);

  base::Value expected_device1(base::Value::Type::DICTIONARY);
  expected_device1.SetIntKey("vendor_id", 1234);
  expected_device1.SetIntKey("product_id", 5678);

  base::Value expected_device2(base::Value::Type::DICTIONARY);
  expected_device2.SetIntKey("vendor_id", 4321);

  base::Value expected_devices(base::Value::Type::LIST);
  expected_devices.Append(std::move(expected_device1));
  expected_devices.Append(std::move(expected_device2));

  base::Value expected_urls(base::Value::Type::LIST);
  expected_urls.Append("https://google.com");
  expected_urls.Append("https://www.youtube.com");

  base::Value expected_item(base::Value::Type::DICTIONARY);
  expected_item.SetKey("devices", std::move(expected_devices));
  expected_item.SetKey("urls", std::move(expected_urls));

  base::Value expected(base::Value::Type::LIST);
  expected.Append(std::move(expected_item));

  EXPECT_EQ(expected, *pref_value);
}

TEST_F(SerialAllowUsbDevicesForUrlsPolicyHandlerTest, MissingUrls) {
  constexpr char kPolicy[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          },
          {"vendor_id": 4321}
        ]
      }
    ])";

  PolicyMap policy;
  policy.Set(key::kSerialAllowUsbDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Schema validation error at \"items[0]\": Missing or invalid required "
      u"property: urls";
  EXPECT_EQ(kExpected, errors.GetErrors(key::kSerialAllowUsbDevicesForUrls));

  // Now try to apply the policy, it should have no effect.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls, nullptr));

  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(SerialAllowUsbDevicesForUrlsPolicyHandlerTest, MissingDevices) {
  constexpr char kPolicy[] = R"(
    [
      {
        "urls": [
          "https://google.com",
          "https://www.youtube.com"
        ]
      }
    ])";

  PolicyMap policy;
  policy.Set(key::kSerialAllowUsbDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Schema validation error at \"items[0]\": Missing or invalid required "
      u"property: devices";
  EXPECT_EQ(kExpected, errors.GetErrors(key::kSerialAllowUsbDevicesForUrls));

  // Now try to apply the policy, it should have no effect.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls, nullptr));

  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(SerialAllowUsbDevicesForUrlsPolicyHandlerTest, DevicesMustBeList) {
  constexpr char kPolicy[] = R"(
    [
      {
        "devices": 1,
        "urls": ["https://chromium.org"]
      }
    ])";

  PolicyMap policy;
  policy.Set(key::kSerialAllowUsbDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Schema validation error at \"items[0].devices\": The value type "
      u"doesn't "
      u"match the schema type.";
  EXPECT_EQ(kExpected, errors.GetErrors(key::kSerialAllowUsbDevicesForUrls));

  // Now try to apply the policy, it should have no effect.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls, nullptr));

  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(SerialAllowUsbDevicesForUrlsPolicyHandlerTest, UrlsMustBeList) {
  constexpr char kPolicy[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          },
          {"vendor_id": 4321}
        ],
        "urls": 1
      }
    ])";

  PolicyMap policy;
  policy.Set(key::kSerialAllowUsbDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Schema validation error at \"items[0].urls\": The value type doesn't "
      u"match the schema type.";
  EXPECT_EQ(kExpected, errors.GetErrors(key::kSerialAllowUsbDevicesForUrls));

  // Now try to apply the policy, it should have no effect.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls, nullptr));

  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(SerialAllowUsbDevicesForUrlsPolicyHandlerTest, VendorIdMustBeInt) {
  constexpr char kPolicy[] = R"(
    [
      {
        "devices": [{"vendor_id": "not_an_int"}],
        "urls": ["https://chromium.org"]
      }
    ])";

  PolicyMap policy;
  policy.Set(key::kSerialAllowUsbDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Schema validation error at \"items[0].devices.items[0].vendor_id\": "
      u"The value type doesn't match the schema type.";
  EXPECT_EQ(kExpected, errors.GetErrors(key::kSerialAllowUsbDevicesForUrls));

  // Now try to apply the policy, it should have no effect.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls, nullptr));

  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(SerialAllowUsbDevicesForUrlsPolicyHandlerTest, VendorIdOutOfRange) {
  constexpr char kPolicy[] = R"(
    [
      {
        "devices": [{"vendor_id": 1000000}],
        "urls": ["https://chromium.org"]
      }
    ])";

  PolicyMap policy;
  policy.Set(key::kSerialAllowUsbDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Schema validation error at \"items[0].devices.items[0].vendor_id\": "
      u"Invalid value for integer";
  EXPECT_EQ(kExpected, errors.GetErrors(key::kSerialAllowUsbDevicesForUrls));

  // Now try to apply the policy, it should have no effect.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls, nullptr));

  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(SerialAllowUsbDevicesForUrlsPolicyHandlerTest,
       ProductIdRequiresVendorId) {
  constexpr char kPolicy[] = R"(
    [
      {
        "devices": [{"product_id": 1234}],
        "urls": ["https://chromium.org"]
      }
    ])";

  PolicyMap policy;
  policy.Set(key::kSerialAllowUsbDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Schema validation error at \"items[0].devices.items[0]\": Missing or "
      u"invalid required property: vendor_id";
  EXPECT_EQ(kExpected, errors.GetErrors(key::kSerialAllowUsbDevicesForUrls));

  // Now try to apply the policy, it should have no effect.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls, nullptr));

  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(SerialAllowUsbDevicesForUrlsPolicyHandlerTest, ProductIdMustBeInt) {
  constexpr char kPolicy[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": "not_an_int"
          }
        ],
        "urls": ["https://chromium.org"]
      }
    ])";

  PolicyMap policy;
  policy.Set(key::kSerialAllowUsbDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Schema validation error at \"items[0].devices.items[0].product_id\": "
      u"The value type doesn't match the schema type.";
  EXPECT_EQ(kExpected, errors.GetErrors(key::kSerialAllowUsbDevicesForUrls));

  // Now try to apply the policy, it should have no effect.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls, nullptr));

  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(SerialAllowUsbDevicesForUrlsPolicyHandlerTest, ProductIdOutOfRange) {
  constexpr char kPolicy[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 1000000
          }
        ],
        "urls": ["https://chromium.org"]
      }
    ])";

  PolicyMap policy;
  policy.Set(key::kSerialAllowUsbDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ReadJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Schema validation error at \"items[0].devices.items[0].product_id\": "
      u"Invalid value for integer";
  EXPECT_EQ(kExpected, errors.GetErrors(key::kSerialAllowUsbDevicesForUrls));

  // Now try to apply the policy, it should have no effect.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls, nullptr));

  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(store_->GetValue(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                &pref_value));
  EXPECT_FALSE(pref_value);
}

}  // namespace policy
