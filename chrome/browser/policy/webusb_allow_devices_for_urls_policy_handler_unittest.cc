// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
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

using ::base::test::IsJson;
using ::base::test::ParseJson;

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

}  // namespace

class WebUsbAllowDevicesForUrlsPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  WebUsbAllowDevicesForUrlsPolicyHandlerTest() = default;
  WebUsbAllowDevicesForUrlsPolicyHandlerTest(
      const WebUsbAllowDevicesForUrlsPolicyHandlerTest&) = delete;
  WebUsbAllowDevicesForUrlsPolicyHandlerTest& operator=(
      const WebUsbAllowDevicesForUrlsPolicyHandlerTest&) = delete;
  ~WebUsbAllowDevicesForUrlsPolicyHandlerTest() override = default;

  WebUsbAllowDevicesForUrlsPolicyHandler* handler() { return handler_; }

 private:
  void SetUp() override {
    Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
    auto handler =
        std::make_unique<WebUsbAllowDevicesForUrlsPolicyHandler>(chrome_schema);
    handler_ = handler.get();
    handler_list_.AddHandler(std::move(handler));
  }

  raw_ptr<WebUsbAllowDevicesForUrlsPolicyHandler> handler_;
};

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest, CheckPolicySettings) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kValidPolicy), nullptr);
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
      ParseJson(kInvalidPolicyInvalidTopLevelEntry), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  static constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[1]: Schema validation error: "
      u"Missing or invalid required property: devices";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMissingDevicesProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyMissingDevicesProperty), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  static constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[0]: Schema validation error: "
      u"Missing or invalid required property: devices";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMissingUrlsProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyMissingUrlsProperty), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  static constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[0]: Schema validation error: "
      u"Missing or invalid required property: urls";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithIntegerUrls) {
  static constexpr char kInvalidPolicy[] = R"(
      [
        {
          "devices": [
            {
              "vendor_id": 1234,
              "product_id": 5678
            }
          ],
          "urls": [
            "https://www.youtube.com",
            42
          ]
        }
      ])";
  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  static constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[0].urls[1]: Schema validation "
      u"error: Policy type mismatch: expected: \"string\", actual: "
      u"\"integer\".";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsUnknownProperty) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyUnknownProperty), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_TRUE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  static constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[0].devices[0]: Schema validation "
      u"error: Unknown property: serialNumber";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMismatchedVendorIdType) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyMismatchedVendorIdType), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const std::u16string kExpected =
      u"Error at WebUsbAllowDevicesForUrls[0].devices[0].vendor_id: Schema "
      u"validation error: Invalid value for integer";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithMismatchedProductIdType) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyMismatchedProductIdType), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  const std::u16string kExpected =
      u"Error at WebUsbAllowDevicesForUrls[0].devices[0].product_id: Schema "
      u"validation error: Invalid value for integer";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithProductIdWithoutVendorId) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyProductIdWithoutVendorId), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  static constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[0].devices[0]: The attribute "
      u"\"vendor_id\" must also be specified.";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidRequestingUrlEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyInvalidRequestingUrl), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  static constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[0].urls[0]: Invalid URL.";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidEmbeddingUrlEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyInvalidEmbeddingUrl), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  static constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[0].urls[0]: Invalid URL.";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithInvalidUrlsEntry) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyInvalidUrlsEntry), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  static constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[0].urls[0]: Each urls string entry "
      u"must contain between 1 to 2 URLs.";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckPolicySettingsWithNoUrls) {
  PolicyMap policy;
  PolicyErrorMap errors;

  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(InvalidPolicyNoUrls), nullptr);

  ASSERT_TRUE(errors.empty());
  EXPECT_FALSE(handler()->CheckPolicySettings(policy, &errors));
  EXPECT_EQ(1ul, errors.size());

  static constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[0].urls[0]: Each urls string entry "
      u"must contain between 1 to 2 URLs.";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest, ApplyPolicySettings) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kValidPolicy), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  ASSERT_TRUE(pref_value);
  ASSERT_TRUE(pref_value->is_list());

  // Ensure that the kManagedWebUsbAllowDevicesForUrls pref is set correctly.
  const auto& list = pref_value->GetList();
  ASSERT_EQ(2ul, list.size());

  // Check the first item's devices list.
  const base::Value::List* first_devices_list =
      list[0].GetDict().FindList(kDevicesKey);
  ASSERT_TRUE(first_devices_list);

  ASSERT_EQ(2ul, first_devices_list->size());

  const base::Value* vendor_id =
      (*first_devices_list)[0].GetDict().Find(kVendorIdKey);
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(1234, vendor_id->GetInt());

  const base::Value* product_id =
      (*first_devices_list)[0].GetDict().Find(kProductIdKey);
  ASSERT_TRUE(product_id);
  EXPECT_EQ(5678, product_id->GetInt());

  vendor_id = (*first_devices_list)[1].GetDict().Find(kVendorIdKey);
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(4321, vendor_id->GetInt());

  product_id = (*first_devices_list)[1].GetDict().Find(kProductIdKey);
  EXPECT_FALSE(product_id);

  // Check the first item's urls list.
  const base::Value::List* first_urls_list =
      list[0].GetDict().FindList(kUrlsKey);
  ASSERT_TRUE(first_urls_list);

  ASSERT_EQ(2ul, first_urls_list->size());
  ASSERT_TRUE((*first_urls_list)[0].is_string());
  ASSERT_TRUE((*first_urls_list)[1].is_string());
  EXPECT_EQ("https://google.com,https://google.com",
            (*first_urls_list)[0].GetString());
  EXPECT_EQ("https://www.youtube.com", (*first_urls_list)[1].GetString());

  // Check the second item's devices list.
  const base::Value::List* second_devices_list =
      list[1].GetDict().FindList(kDevicesKey);
  ASSERT_TRUE(second_devices_list);

  ASSERT_EQ(1ul, second_devices_list->size());

  vendor_id = (*second_devices_list)[0].GetDict().Find(kVendorIdKey);
  EXPECT_FALSE(vendor_id);

  product_id = (*second_devices_list)[0].GetDict().Find(kProductIdKey);
  EXPECT_FALSE(product_id);

  // Check the second item's urls list.
  const base::Value::List* second_urls_list =
      list[1].GetDict().FindList(kUrlsKey);
  ASSERT_TRUE(second_urls_list);

  ASSERT_EQ(1ul, second_urls_list->size());
  ASSERT_TRUE((*second_urls_list)[0].is_string());
  EXPECT_EQ("https://chromium.org,", (*second_urls_list)[0].GetString());
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithInvalidTopLevelEntry) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyInvalidTopLevelEntry), nullptr);
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
      ParseJson(kInvalidPolicyMissingDevicesProperty), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMissingUrlsProperty) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyMissingUrlsProperty), nullptr);
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
      ParseJson(kInvalidPolicyUnknownProperty), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_TRUE(pref_value);

  std::optional<base::Value> expected_pref_value =
      ParseJson(kInvalidPolicyUnknownPropertyAfterCleanup);
  EXPECT_EQ(*expected_pref_value, *pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsWithMismatchedVendorIdType) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyMismatchedVendorIdType), nullptr);
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
      ParseJson(kInvalidPolicyMismatchedProductIdType), nullptr);
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
      ParseJson(kInvalidPolicyProductIdWithoutVendorId), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsInvalidRequestingUrl) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyInvalidRequestingUrl), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsInvalidEmbeddingUrl) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyInvalidEmbeddingUrl), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       ApplyPolicySettingsInvalidUrlsEntry) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kInvalidPolicyInvalidUrlsEntry), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest, ApplyPolicySettingsNoUrls) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebUsbAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(InvalidPolicyNoUrls), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  EXPECT_FALSE(pref_value);
}

TEST_F(WebUsbAllowDevicesForUrlsPolicyHandlerTest,
       CheckAndApplyPolicySettingsWithUnknownTopLevelKey) {
  // A policy with an unknown top-level key should generate an error but have
  // the unknown key removed during normaliziation and be applied successfully.
  constexpr char kPolicy[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          }
        ],
        "unknown_top_level_property": true,
        "urls": [
          "https://www.youtube.com"
        ]
      }
    ]
  )";
  constexpr char kNormalizedPolicy[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          }
        ],
        "urls": [
          "https://www.youtube.com"
        ]
      }
    ]
  )";

  PolicyMap policy;
  policy.Set(key::kWebUsbAllowDevicesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(kPolicy), nullptr);

  PolicyErrorMap errors;
  EXPECT_TRUE(errors.empty());
  EXPECT_TRUE(handler()->CheckPolicySettings(policy, &errors));

  constexpr char16_t kExpected[] =
      u"Error at WebUsbAllowDevicesForUrls[0]: Schema validation error: "
      u"Unknown property: unknown_top_level_property";
  EXPECT_EQ(kExpected,
            errors.GetErrorMessages(key::kWebUsbAllowDevicesForUrls));

  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, nullptr));
  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(
      store_->GetValue(prefs::kManagedWebUsbAllowDevicesForUrls, &pref_value));
  ASSERT_TRUE(pref_value);

  EXPECT_THAT(*pref_value, IsJson(kNormalizedPolicy));
}

}  // namespace policy
