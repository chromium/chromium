// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/value_provider/chrome_policies_value_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/policy/cloud/mock_extension_install_policy_service.h"
#include "chrome/browser/policy/value_provider/extension_install_policies_value_provider.h"
#include "chrome/browser/policy/value_provider/extension_policies_value_provider.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/l10n/l10n_util.h"
#endif

using testing::_;
using testing::ReturnRef;

namespace policy {

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace em = enterprise_management;
constexpr char kExtensionId[] = "abcdefghijklmnoabcdefghijklmnoab";
#endif

class PolicyValueProviderTestBase : public testing::Test {
 public:
  PolicyValueProviderTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~PolicyValueProviderTestBase() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    auto policy_service =
        std::make_unique<testing::NiceMock<MockPolicyService>>();
    policy_service_ = policy_service.get();
    ON_CALL(*policy_service_, GetPolicies(_))
        .WillByDefault(ReturnRef(policy_map_));

    profile_ = profile_manager_.CreateTestingProfile(
        "test_profile", /*prefs=*/nullptr, u"test_profile", 0, {},
        /*is_supervised_profile=*/false, /*is_new_profile=*/std::nullopt,
        std::move(policy_service));
  }

  void TearDown() override {
    policy_service_ = nullptr;
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

 protected:
  void RegisterSchema(PolicyDomain domain,
                      const std::string& component_id,
                      const std::string& schema_json) {
    base::expected<Schema, std::string> schema = Schema::Parse(schema_json);
    ASSERT_TRUE(schema.has_value()) << schema.error();
    ComponentMap components;
    components[component_id] = schema.value();
    profile_->GetPolicySchemaRegistryService()->registry()->RegisterComponents(
        domain, components);
  }

  void SetPolicy(PolicyMap& map, const std::string& name, base::Value value) {
    map.Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
            POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  }

  const base::DictValue* GetPolicyDict(const base::DictValue& values,
                                       const std::string& provider_id,
                                       const std::string& policy_name) {
    const base::DictValue* provider_dict = values.FindDict(provider_id);
    if (!provider_dict) {
      return nullptr;
    }
    const base::DictValue* policies = provider_dict->FindDict(kPoliciesKey);
    if (!policies) {
      return nullptr;
    }
    return policies->FindDict(policy_name);
  }

  void VerifyPolicyValue(const base::DictValue& values,
                         const std::string& provider_id,
                         const std::string& policy_name,
                         const base::Value& expected_value) {
    const base::DictValue* policy_dict =
        GetPolicyDict(values, provider_id, policy_name);
    ASSERT_TRUE(policy_dict) << "Policy " << policy_name << " not found";
    const base::Value* value = policy_dict->Find("value");
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, expected_value);
  }

  void VerifyProviderNames(
      const base::DictValue& names,
      const std::string& provider_id,
      const std::string& expected_name,
      const std::vector<std::string>& expected_policy_names) {
    const base::DictValue* provider_dict = names.FindDict(provider_id);
    ASSERT_TRUE(provider_dict) << "Provider " << provider_id << " not found";
    EXPECT_EQ(*provider_dict->FindString(kNameKey), expected_name);
    const base::ListValue* policy_names =
        provider_dict->FindList(kPolicyNamesKey);
    ASSERT_TRUE(policy_names);
    EXPECT_THAT(*policy_names,
                testing::UnorderedElementsAreArray(expected_policy_names));
  }

  TestingProfileManager profile_manager_;
  content::BrowserTaskEnvironment task_environment_;
  PolicyMap policy_map_;
  raw_ptr<MockPolicyService> policy_service_;
  raw_ptr<TestingProfile> profile_;
};

class ChromePoliciesValueProviderTest : public PolicyValueProviderTestBase {
 public:
  void SetUp() override {
    PolicyValueProviderTestBase::SetUp();

    // Set up a schema for Chrome policies.
    RegisterSchema(POLICY_DOMAIN_CHROME, "", R"(
        {
          "type": "object",
          "properties": {
            "ShowHomeButton": { "type": "boolean" }
          }
        }
    )");
  }
};

TEST_F(ChromePoliciesValueProviderTest, GetValues) {
  SetPolicy(policy_map_, key::kShowHomeButton, base::Value(true));

  ChromePoliciesValueProvider provider(profile_.get());
  VerifyPolicyValue(provider.GetValues(), kChromePoliciesId,
                    key::kShowHomeButton, base::Value(true));
}

TEST_F(ChromePoliciesValueProviderTest, GetNames) {
  ChromePoliciesValueProvider provider(profile_.get());
  base::DictValue names = provider.GetNames();

  VerifyProviderNames(names, kChromePoliciesId, kChromePoliciesName,
                      {key::kShowHomeButton});

#if !BUILDFLAG(IS_CHROMEOS)
  VerifyProviderNames(
      names, kPrecedencePoliciesId, kPrecedencePoliciesName,
      std::vector<std::string>(std::begin(metapolicy::kPrecedence),
                               std::end(metapolicy::kPrecedence)));
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_CHROMEOS)
class ExtensionInstallPoliciesValueProviderTest
    : public PolicyValueProviderTestBase {
 public:
  void SetUp() override {
    PolicyValueProviderTestBase::SetUp();

    profile_->GetPrefs()->SetBoolean(
        extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled,
        true);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kEnableExtensionInstallPolicyFetching};
  testing::NiceMock<MockExtensionInstallPolicyService> mock_service_;
};

TEST_F(ExtensionInstallPoliciesValueProviderTest, GetValues) {
  SetPolicy(
      policy_map_, "extension_id",
      base::Value(base::DictValue().Set(
          "1.2.3",
          base::DictValue()
              .Set("action", em::ExtensionInstallPolicy::ACTION_BLOCK)
              .Set("reasons",
                   base::ListValue().Append(
                       em::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY)))));

  EXPECT_CALL(*policy_service_,
              GetPolicies(PolicyNamespace(POLICY_DOMAIN_EXTENSION_INSTALL,
                                          std::string())))
      .WillOnce(ReturnRef(policy_map_));

  ExtensionInstallPoliciesValueProvider provider(profile_.get(),
                                                 &mock_service_);
  base::DictValue values = provider.GetValues();
  const base::DictValue* policy =
      GetPolicyDict(values, kExtensionInstallPoliciesId, "extension_id@1.2.3");
  ASSERT_TRUE(policy);
  EXPECT_EQ(*policy,
            base::DictValue()
                .Set("level", "mandatory")
                .Set("scope", "user")
                .Set("source", "cloud")
                .Set("value", base::DictValue()
                                  .Set("action", "block")
                                  .Set("reasons",
                                       base::ListValue().Append("category"))));
}

TEST_F(ExtensionInstallPoliciesValueProviderTest, GetNames) {
  ExtensionInstallPoliciesValueProvider provider(profile_.get(),
                                                 &mock_service_);
  VerifyProviderNames(provider.GetNames(), kExtensionInstallPoliciesId,
                      kExtensionInstallPoliciesName, {});
}

TEST_F(ExtensionInstallPoliciesValueProviderTest,
       GetValues_IgnoredByInstallationMode) {
  SetPolicy(policy_map_, kExtensionId,
            base::Value(base::DictValue().Set(
                "1.2.3",
                base::DictValue()
                    .Set("action", em::ExtensionInstallPolicy::ACTION_BLOCK)
                    .Set("reasons",
                         base::ListValue().Append(
                             em::ExtensionInstallPolicy::REASON_RISK_SCORE)))));

  EXPECT_CALL(*policy_service_,
              GetPolicies(PolicyNamespace(POLICY_DOMAIN_EXTENSION_INSTALL,
                                          std::string())))
      .WillRepeatedly(ReturnRef(policy_map_));

  // Explicitly allow the extension.
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        pref_updater(profile_->GetTestingPrefService());
    pref_updater.SetIndividualExtensionInstallationAllowed(kExtensionId, true);
  }

  ExtensionInstallPoliciesValueProvider provider(profile_.get(),
                                                 &mock_service_);
  base::DictValue values = provider.GetValues();
  const base::DictValue* policy =
      GetPolicyDict(values, kExtensionInstallPoliciesId,
                    absl::StrFormat("%s@1.2.3", kExtensionId));
  ASSERT_TRUE(policy);
  EXPECT_TRUE(policy->FindBool("ignored").value_or(false));
  EXPECT_EQ(base::UTF8ToUTF16(*policy->FindString("info")),
            l10n_util::GetStringUTF16(
                IDS_POLICY_EXTENSION_INSTALL_IGNORED_BY_INSTALLATION_MODE));
}
#endif  // BUIDLFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionPoliciesValueProviderTest : public PolicyValueProviderTestBase {
 public:
  void SetUp() override {
    PolicyValueProviderTestBase::SetUp();

    // Use a separate map for extension policies to match original test.
    ON_CALL(*policy_service_, GetPolicies(_))
        .WillByDefault(ReturnRef(empty_policy_map_));

    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(profile_.get());
    extension_registry->AddEnabled(
        extensions::ExtensionBuilder("extension_name")
            .SetID(kExtensionId)
            .SetManifestPath(extensions::manifest_keys::kStorageManagedSchema,
                             "schema.json")
            .Build());

    // Set up a schema for the extension.
    RegisterSchema(POLICY_DOMAIN_EXTENSIONS, kExtensionId, R"(
        {
          "type": "object",
          "properties": {
            "policy_a": { "type": "integer" }
          }
        }
    )");
  }

 protected:
  PolicyMap empty_policy_map_;
  PolicyMap extension_policy_map_;
};

TEST_F(ExtensionPoliciesValueProviderTest, GetValues) {
  SetPolicy(extension_policy_map_, "policy_a", base::Value(123));

  EXPECT_CALL(*policy_service_, GetPolicies(PolicyNamespace(
                                    POLICY_DOMAIN_EXTENSIONS, kExtensionId)))
      .WillRepeatedly(ReturnRef(extension_policy_map_));

  ExtensionPoliciesValueProvider provider(profile_.get());
  VerifyPolicyValue(provider.GetValues(), kExtensionId, "policy_a",
                    base::Value(123));
}

TEST_F(ExtensionPoliciesValueProviderTest, GetNames) {
  ExtensionPoliciesValueProvider provider(profile_.get());
  VerifyProviderNames(provider.GetNames(), kExtensionId, "extension_name",
                      {"policy_a"});
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

}  // namespace policy
