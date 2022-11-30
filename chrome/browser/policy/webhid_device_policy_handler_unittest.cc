// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/webhid_device_policy_handler.h"

#include <memory>
#include <utility>

#include "base/strings/string_util.h"
#include "base/test/values_test_util.h"
#include "chrome/common/pref_names.h"
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
using ::testing::WithParamInterface;

constexpr char kDevicesKey[] = "devices";
constexpr char kUsagesKey[] = "usages";
constexpr char kUrlsKey[] = "urls";
constexpr char kVendorIdKey[] = "vendor_id";
constexpr char kProductIdKey[] = "product_id";
constexpr char kUsagePageKey[] = "usage_page";
constexpr char kUsageKey[] = "usage";

}  // namespace

class WebHidDevicePolicyHandlerTest : public ConfigurationPolicyPrefStoreTest {
 public:
  WebHidDevicePolicyHandlerTest() = default;
  WebHidDevicePolicyHandlerTest(const WebHidDevicePolicyHandlerTest&) = delete;
  WebHidDevicePolicyHandlerTest& operator=(
      const WebHidDevicePolicyHandlerTest&) = delete;
  ~WebHidDevicePolicyHandlerTest() override = default;

 protected:
  SchemaValidatingPolicyHandler* AddHandler(const char* policy_name) {
    auto handler = CreateHandler(policy_name);
    EXPECT_TRUE(handler) << "no policy handler for " << policy_name;
    if (!handler)
      return nullptr;

    auto* handler_ptr = handler.get();
    handler_list_.AddHandler(std::move(handler));
    return handler_ptr;
  }

 private:
  // Returns the appropriate handler for `policy_name`, which must be a
  // `policy::key::` constant.
  std::unique_ptr<SchemaValidatingPolicyHandler> CreateHandler(
      const char* policy_name) {
    Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
    if (policy_name == key::kWebHidAllowDevicesForUrls) {
      return std::make_unique<WebHidDevicePolicyHandler>(
          key::kWebHidAllowDevicesForUrls,
          prefs::kManagedWebHidAllowDevicesForUrls, chrome_schema);
    }
    if (policy_name == key::kWebHidAllowDevicesWithHidUsagesForUrls) {
      return std::make_unique<WebHidDevicePolicyHandler>(
          key::kWebHidAllowDevicesWithHidUsagesForUrls,
          prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls, chrome_schema);
    }
    return nullptr;
  }
};

namespace {

constexpr uint16_t kTestVendorId1 = 1234;
constexpr uint16_t kTestVendorId2 = 4321;
constexpr uint16_t kTestVendorId3 = 2000;

constexpr uint16_t kTestProductId = 5678;

constexpr uint16_t kTestUsagePage1 = 1;
constexpr uint16_t kTestUsagePage2 = 2;
constexpr uint16_t kTestUsagePage3 = 3;

constexpr uint16_t kTestUsage = 4;

constexpr char kAllowDevicesForUrls[] = R"(
    [
      {
        "devices": [
          {
            "vendor_id": 1234,
            "product_id": 5678
          },
          {
            "vendor_id": 4321
          }
        ],
        "urls": [
          "https://origin1",
          "https://origin2"
        ]
      }, {
        "devices": [
          {
            "vendor_id": 2000
          }
        ],
        "urls": [
          "https://origin3"
        ]
      }
    ])";

constexpr char kAllowDevicesWithHidUsagesForUrls[] = R"(
    [
      {
        "usages": [
          {
            "usage_page": 1,
            "usage": 4
          },
          {
            "usage_page": 2
          }
        ],
        "urls": [
          "https://origin1",
          "https://origin2"
        ]
      }, {
        "usages": [
          {
            "usage_page": 3
          }
        ],
        "urls": [
          "https://origin3"
        ]
      }
    ])";

}  // namespace

TEST_F(WebHidDevicePolicyHandlerTest, CheckPolicySettingsWithDevicePolicy) {
  auto* handler = AddHandler(key::kWebHidAllowDevicesForUrls);

  PolicyMap policy;
  PolicyErrorMap errors;
  policy.Set(
      key::kWebHidAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kAllowDevicesForUrls), /*external_data_fetcher=*/nullptr);
  ASSERT_TRUE(errors.empty());
  EXPECT_TRUE(handler->CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(WebHidDevicePolicyHandlerTest, CheckPolicySettingsWithUsagePolicy) {
  auto* handler = AddHandler(key::kWebHidAllowDevicesWithHidUsagesForUrls);

  PolicyMap policy;
  PolicyErrorMap errors;
  policy.Set(key::kWebHidAllowDevicesWithHidUsagesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ParseJson(kAllowDevicesWithHidUsagesForUrls),
             /*external_data_fetcher=*/nullptr);
  ASSERT_TRUE(errors.empty());
  EXPECT_TRUE(handler->CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(WebHidDevicePolicyHandlerTest, ApplyPolicySettingsWithDevicePolicy) {
  AddHandler(key::kWebHidAllowDevicesForUrls);
  EXPECT_FALSE(store_->GetValue(prefs::kManagedWebHidAllowDevicesForUrls,
                                /*result=*/nullptr));

  PolicyMap policy;
  policy.Set(
      key::kWebHidAllowDevicesForUrls, PolicyLevel::POLICY_LEVEL_MANDATORY,
      PolicyScope::POLICY_SCOPE_MACHINE, PolicySource::POLICY_SOURCE_CLOUD,
      ParseJson(kAllowDevicesForUrls), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(
      store_->GetValue(prefs::kManagedWebHidAllowDevicesForUrls, &pref_value));
  ASSERT_TRUE(pref_value);
  ASSERT_TRUE(pref_value->is_list());

  // Ensure that the kManagedWebHidAllowDevicesForUrls pref is set correctly.
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
  EXPECT_EQ(base::Value(kTestVendorId1), *vendor_id);

  const base::Value* product_id =
      (*first_devices_list)[0].GetDict().Find(kProductIdKey);
  ASSERT_TRUE(product_id);
  EXPECT_EQ(base::Value(kTestProductId), *product_id);

  vendor_id = (*first_devices_list)[1].GetDict().Find(kVendorIdKey);
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(base::Value(kTestVendorId2), *vendor_id);

  product_id = (*first_devices_list)[1].GetDict().Find(kProductIdKey);
  EXPECT_FALSE(product_id);

  // Check the first item's urls list.
  const base::Value::List* urls = list[0].GetDict().FindList(kUrlsKey);
  ASSERT_TRUE(urls);
  ASSERT_EQ(2ul, urls->size());
  EXPECT_EQ(base::Value("https://origin1"), (*urls)[0]);
  EXPECT_EQ(base::Value("https://origin2"), (*urls)[1]);

  // Check the second item's devices list.
  const base::Value::List* second_devices_list =
      list[1].GetDict().FindList(kDevicesKey);
  ASSERT_TRUE(second_devices_list);

  ASSERT_EQ(1ul, second_devices_list->size());

  vendor_id = (*second_devices_list)[0].GetDict().Find(kVendorIdKey);
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(base::Value(kTestVendorId3), *vendor_id);

  product_id = (*second_devices_list)[0].GetDict().Find(kProductIdKey);
  EXPECT_FALSE(product_id);

  // Check the second item's urls list.
  urls = list[1].GetDict().FindList(kUrlsKey);
  ASSERT_TRUE(urls);
  ASSERT_EQ(1ul, urls->size());
  EXPECT_EQ(base::Value("https://origin3"), (*urls)[0]);
}

TEST_F(WebHidDevicePolicyHandlerTest, ApplyPolicySettingsWithUsagePolicy) {
  AddHandler(key::kWebHidAllowDevicesWithHidUsagesForUrls);
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
                       /*result=*/nullptr));

  PolicyMap policy;
  policy.Set(key::kWebHidAllowDevicesWithHidUsagesForUrls,
             PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD,
             ParseJson(kAllowDevicesWithHidUsagesForUrls), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(store_->GetValue(
      prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls, &pref_value));
  ASSERT_TRUE(pref_value);
  ASSERT_TRUE(pref_value->is_list());

  // Ensure that the kManagedWebHidAllowDevicesWithHidUsagesForUrls pref is set
  // correctly.
  const auto& list = pref_value->GetList();
  ASSERT_EQ(2ul, list.size());

  // Check the first item's usages list.
  const base::Value::List* first_usages_list =
      list[0].GetDict().FindList(kUsagesKey);
  ASSERT_TRUE(first_usages_list);

  ASSERT_EQ(2ul, first_usages_list->size());

  const base::Value* usage_page =
      (*first_usages_list)[0].GetDict().Find(kUsagePageKey);
  ASSERT_TRUE(usage_page);
  EXPECT_EQ(base::Value(kTestUsagePage1), *usage_page);

  const base::Value* usage = (*first_usages_list)[0].GetDict().Find(kUsageKey);
  ASSERT_TRUE(usage);
  EXPECT_EQ(base::Value(kTestUsage), *usage);

  usage_page = (*first_usages_list)[1].GetDict().Find(kUsagePageKey);
  ASSERT_TRUE(usage_page);
  EXPECT_EQ(base::Value(kTestUsagePage2), *usage_page);

  usage = (*first_usages_list)[1].GetDict().Find(kUsageKey);
  EXPECT_FALSE(usage);

  // Check the first item's urls list.
  const base::Value::List* urls = list[0].GetDict().FindList(kUrlsKey);
  ASSERT_TRUE(urls);
  ASSERT_EQ(2ul, urls->size());
  EXPECT_EQ(base::Value("https://origin1"), (*urls)[0]);
  EXPECT_EQ(base::Value("https://origin2"), (*urls)[1]);

  // Check the second item's usages list.
  const base::Value::List* second_usages_list =
      list[1].GetDict().FindList(kUsagesKey);
  ASSERT_TRUE(second_usages_list);

  ASSERT_EQ(1ul, second_usages_list->size());

  usage_page = (*second_usages_list)[0].GetDict().Find(kUsagePageKey);
  ASSERT_TRUE(usage_page);
  EXPECT_EQ(base::Value(kTestUsagePage3), *usage_page);

  usage = (*second_usages_list)[0].GetDict().Find(kUsageKey);
  EXPECT_FALSE(usage);

  // Check the second item's urls list.
  urls = list[1].GetDict().FindList(kUrlsKey);
  ASSERT_TRUE(urls);
  ASSERT_EQ(1ul, urls->size());
  EXPECT_EQ(base::Value("https://origin3"), (*urls)[0]);
}

struct WebHidInvalidPolicyTestData {
  const char* policy_name;
  const char* pref_name;
  const char* policy;
  const char16_t* expected_errors;
  const char* expected_pref;
};

class WebHidInvalidPolicyTest
    : public WebHidDevicePolicyHandlerTest,
      public WithParamInterface<WebHidInvalidPolicyTestData> {};

TEST_P(WebHidInvalidPolicyTest, CheckPolicySettingsWithInvalidPolicy) {
  const auto& test_data = GetParam();
  auto* handler = AddHandler(test_data.policy_name);

  PolicyMap policy;
  policy.Set(test_data.policy_name, PolicyLevel::POLICY_LEVEL_MANDATORY,
             PolicyScope::POLICY_SCOPE_MACHINE,
             PolicySource::POLICY_SOURCE_CLOUD, ParseJson(test_data.policy),
             /*external_data_fetcher=*/nullptr);

  // Try CheckPolicySettings with the invalid policy. It returns success if the
  // policy can be successfully applied, even if there are errors.
  PolicyErrorMap errors;
  bool success = handler->CheckPolicySettings(policy, &errors);
  EXPECT_EQ(success, test_data.expected_pref != nullptr);
  EXPECT_EQ(test_data.expected_errors,
            errors.GetErrorMessages(test_data.policy_name));

  EXPECT_FALSE(store_->GetValue(test_data.pref_name, /*result=*/nullptr));

  // Try updating the policy.
  UpdateProviderPolicy(policy);

  // Check that the preference has the expected value.
  if (test_data.expected_pref) {
    const base::Value* pref_value = nullptr;
    EXPECT_TRUE(store_->GetValue(test_data.pref_name, &pref_value));
    ASSERT_TRUE(pref_value);
    EXPECT_THAT(*pref_value, IsJson(test_data.expected_pref));
  } else {
    EXPECT_FALSE(store_->GetValue(test_data.pref_name, /*result=*/nullptr));
  }
}

WebHidInvalidPolicyTestData kTestData[]{
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [
          {
            "urls": [
              "https://google.com"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesForUrls[0]: Schema validation error: "
        u"Missing or invalid required property: devices",
        "[]",
    },
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234
              }
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesForUrls[0]: Schema validation error: "
        u"Missing or invalid required property: urls",
        "[]",
    },
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234,
                "serial_number": "1234ABCD"
              }
            ],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesForUrls[0].devices[0]: Schema validation "
        u"error: Unknown property: serial_number",
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234
              }
            ],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 65536
              }
            ],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesForUrls[0].devices[0].vendor_id: Schema "
        u"validation error: Invalid value for integer",
        R"(
        [
          {
            "devices": [],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234,
                "product_id": 65536
              }
            ],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesForUrls[0].devices[0].product_id: Schema "
        u"validation error: Invalid value for integer",
        R"(
        [
          {
            "devices": [],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [
          {
            "devices": [
              {
                "product_id": 1234
              }
            ],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesForUrls[0].devices[0]: Schema validation "
        u"error: Missing or invalid required property: vendor_id",
        R"(
        [
          {
            "devices": [],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234
              }
            ],
            "urls": [
              "not-a-valid-url"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesForUrls[0].urls[0]: Invalid URL.",
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234
              }
            ],
            "urls": [
              "not-a-valid-url"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234
              }
            ],
            "urls": [
              ""
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesForUrls[0].urls[0]: Invalid URL.",
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234
              }
            ],
            "urls": [
              ""
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234
              }
            ],
            "urls": [
              "invalid-url-1",
              "invalid-url-2"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesForUrls[0].urls[0]: Invalid URL.\n"
        u"Error at WebHidAllowDevicesForUrls[0].urls[1]: Invalid URL.",
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234
              }
            ],
            "urls": [
              "invalid-url-1",
              "invalid-url-2"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234
              }
            ],
            "urls": [
              123
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesForUrls[0].urls[0]: Schema validation "
        u"error: Policy type mismatch: expected: \"string\", actual: "
        u"\"integer\".\nError at WebHidAllowDevicesForUrls[0].urls[0]: Invalid "
        u"URL.",
        R"(
        [
          {
            "devices": [
              {
                "vendor_id": 1234
              }
            ],
            "urls": [
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesForUrls,
        prefs::kManagedWebHidAllowDevicesForUrls,
        R"(
        [123]
        )",
        u"Error at WebHidAllowDevicesForUrls[0]: Schema validation error: "
        u"Policy type mismatch: expected: \"dictionary\", actual: \"integer\".",
        R"(
        []
        )",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [
          {
            "urls": [
              "https://google.com"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0]: Schema "
        u"validation error: Missing or invalid required property: usages",
        "[]",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234
              }
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0]: Schema "
        u"validation "
        u"error: Missing or invalid required property: urls",
        "[]",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234,
                "serial_number": "1234ABCD"
              }
            ],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0].usages[0]: "
        u"Schema "
        u"validation error: Unknown property: serial_number",
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234
              }
            ],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 65536
              }
            ],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
        u"Error at "
        u"WebHidAllowDevicesWithHidUsagesForUrls[0].usages[0].usage_page: "
        u"Schema validation error: Invalid value for integer",
        R"(
        [
          {
            "usages": [],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234,
                "usage": 65536
              }
            ],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0].usages[0].usage: "
        u"Schema validation error: Invalid value for integer",
        R"(
        [
          {
            "usages": [],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [
          {
            "usages": [
              {
                "usage": 1234
              }
            ],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0].usages[0]: Schema "
        u"validation error: Missing or invalid required property: usage_page",
        R"(
        [
          {
            "usages": [],
            "urls": [
              "https://google.com"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234
              }
            ],
            "urls": [
              "not-a-valid-url"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0].urls[0]: Invalid "
        u"URL.",
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234
              }
            ],
            "urls": [
              "not-a-valid-url"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234
              }
            ],
            "urls": [
              ""
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0].urls[0]: Invalid "
        u"URL.",
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234
              }
            ],
            "urls": [
              ""
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234
              }
            ],
            "urls": [
              "invalid-url-1",
              "invalid-url-2"
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0].urls[0]: Invalid "
        u"URL.\n"
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0].urls[1]: Invalid "
        u"URL.",
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234
              }
            ],
            "urls": [
              "invalid-url-1",
              "invalid-url-2"
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234
              }
            ],
            "urls": [
              123
            ]
          }
        ])",
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0].urls[0]: Schema "
        u"validation error: Policy type mismatch: expected: \"string\", "
        u"actual: \"integer\".\nError at "
        u"WebHidAllowDevicesWithHidUsagesForUrls[0].urls[0]: Invalid URL.",
        R"(
        [
          {
            "usages": [
              {
                "usage_page": 1234
              }
            ],
            "urls": [
            ]
          }
        ])",
    },
    {
        key::kWebHidAllowDevicesWithHidUsagesForUrls,
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        R"(
        [123]
        )",
        u"Error at WebHidAllowDevicesWithHidUsagesForUrls[0]: Schema "
        u"validation error: Policy type mismatch: expected: \"dictionary\", "
        u"actual: \"integer\".",
        R"(
        []
        )",
    },
};

INSTANTIATE_TEST_SUITE_P(WebHidInvalidPolicyTests,
                         WebHidInvalidPolicyTest,
                         testing::ValuesIn(kTestData));

}  // namespace policy
