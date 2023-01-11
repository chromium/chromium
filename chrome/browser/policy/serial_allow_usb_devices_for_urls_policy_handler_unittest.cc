// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/values_test_util.h"
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

using ::base::test::ParseJson;

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

  raw_ptr<ConfigurationPolicyHandler> handler_;
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
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

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

  base::Value::Dict expected_device1;
  expected_device1.Set("vendor_id", 1234);
  expected_device1.Set("product_id", 5678);

  base::Value::Dict expected_device2;
  expected_device2.Set("vendor_id", 4321);

  base::Value::List expected_devices;
  expected_devices.Append(std::move(expected_device1));
  expected_devices.Append(std::move(expected_device2));

  base::Value::List expected_urls;
  expected_urls.Append("https://google.com");
  expected_urls.Append("https://www.youtube.com");

  base::Value::Dict expected_item;
  expected_item.Set("devices", std::move(expected_devices));
  expected_item.Set("urls", std::move(expected_urls));

  base::Value::List expected;
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
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Error at SerialAllowUsbDevicesForUrls[0]: Schema validation error: "
      u"Missing or invalid required property: urls";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kSerialAllowUsbDevicesForUrls));

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
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Error at SerialAllowUsbDevicesForUrls[0]: Schema validation error: "
      u"Missing or invalid required property: devices";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kSerialAllowUsbDevicesForUrls));

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
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Error at SerialAllowUsbDevicesForUrls[0].devices: Schema validation "
      u"error: Policy type mismatch: expected: \"list\", actual: \"integer\".";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kSerialAllowUsbDevicesForUrls));

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
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Error at SerialAllowUsbDevicesForUrls[0].urls: Schema validation "
      u"error: Policy type mismatch: expected: \"list\", actual: \"integer\".";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kSerialAllowUsbDevicesForUrls));

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
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Error at SerialAllowUsbDevicesForUrls[0].devices[0].vendor_id: Schema "
      u"validation error: Policy type mismatch: expected: \"integer\", actual: "
      u"\"string\".";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kSerialAllowUsbDevicesForUrls));

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
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Error at SerialAllowUsbDevicesForUrls[0].devices[0].vendor_id: Schema "
      u"validation error: Invalid value for integer";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kSerialAllowUsbDevicesForUrls));

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
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Error at SerialAllowUsbDevicesForUrls[0].devices[0]: Schema validation "
      u"error: Missing or invalid required property: vendor_id";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kSerialAllowUsbDevicesForUrls));

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
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Error at SerialAllowUsbDevicesForUrls[0].devices[0].product_id: Schema "
      u"validation error: Policy type mismatch: expected: \"integer\", actual: "
      u"\"string\".";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kSerialAllowUsbDevicesForUrls));

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
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  constexpr char16_t kExpected[] =
      u"Error at SerialAllowUsbDevicesForUrls[0].devices[0].product_id: Schema "
      u"validation error: Invalid value for integer";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kSerialAllowUsbDevicesForUrls));

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
