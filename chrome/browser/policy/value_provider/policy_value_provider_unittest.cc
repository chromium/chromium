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

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/policy/value_provider/web_app_managed_configuration_value_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/pref_names.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#endif

using testing::_;
using testing::Eq;
using testing::Pointee;
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
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

#if !BUILDFLAG(IS_ANDROID)
constexpr char kTestIwaAppName[] = "Test Isolated Web App";

class TestPolicyValueProviderObserver : public PolicyValueProvider::Observer {
 public:
  void OnPolicyValueChanged() override { policy_values_changed_count_++; }

  int policy_values_changed_count() const {
    return policy_values_changed_count_;
  }

 private:
  int policy_values_changed_count_ = 0;
};

class WebAppManagedConfigurationValueProviderTest
    : public PolicyValueProviderTestBase {
 public:
  void SetUp() override {
    PolicyValueProviderTestBase::SetUp();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());
  }

 protected:
  ManagedConfigurationAPI* managed_config_api() {
    return ManagedConfigurationAPIFactory::GetForProfile(profile_.get());
  }

  web_app::IsolatedWebAppUrlInfo InstallTestIwa(
      const std::string& app_name = kTestIwaAppName) {
    std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetName(app_name).SetVersion("1.0.0"))
            .BuildBundle();
    return app->InstallChecked(profile_.get());
  }

  void SetManagedConfigurationPref(const url::Origin& origin) {
    base::ListValue configs = base::ListValue().Append(base::DictValue().Set(
        ManagedConfigurationAPI::kOriginKey, origin.GetURL().spec()));

    profile_->GetPrefs()->SetList(prefs::kManagedConfigurationPerOrigin,
                                  std::move(configs));
  }

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(WebAppManagedConfigurationValueProviderTest,
       GetValues_EmptyWhenNoManagedConfigurations) {
  auto provider =
      WebAppManagedConfigurationValueProvider::Create(profile_.get());
  ASSERT_TRUE(provider);

  EXPECT_TRUE(provider->GetValues().empty());
  EXPECT_TRUE(provider->GetNames().empty());
}

TEST_F(WebAppManagedConfigurationValueProviderTest,
       GetValues_ReturnsIwaManagedConfiguration) {
  web_app::IsolatedWebAppUrlInfo url_info = InstallTestIwa();
  const std::string serialized_origin = url_info.origin().Serialize();
  const std::string scope_key = url_info.origin().GetURL().spec();
  SetManagedConfigurationPref(url_info.origin());

  SetPolicy(policy_map_, key::kManagedConfigurationPerOrigin,
            base::Value(base::ListValue()));

  auto provider =
      WebAppManagedConfigurationValueProvider::Create(profile_.get());
  ASSERT_TRUE(provider);

  base::DictValue config;
  config.Set("config_str", "test_value");
  config.Set("config_int", 42);
  managed_config_api()->SetConfigurationForTesting(url_info.origin(),
                                                   std::move(config));
  task_environment_.RunUntilIdle();

  base::DictValue values = provider->GetValues();
  const base::DictValue* iwa_table = values.FindDict(scope_key);
  ASSERT_TRUE(iwa_table);
  EXPECT_THAT(iwa_table->FindString(kNameKey), Pointee(Eq(kTestIwaAppName)));
  EXPECT_THAT(iwa_table->FindString(kIdKey), Pointee(Eq(serialized_origin)));
  EXPECT_EQ(iwa_table->FindBool("isExtension"), false);
  EXPECT_EQ(iwa_table->FindBool("isWebApp"), true);

  const base::DictValue* config_entries = iwa_table->FindDict(kPoliciesKey);
  ASSERT_TRUE(config_entries);

  const base::DictValue* config_str = config_entries->FindDict("config_str");
  ASSERT_TRUE(config_str);
  EXPECT_THAT(config_str->FindString("value"), Pointee(Eq("test_value")));
  EXPECT_THAT(config_str->FindString("level"), Pointee(Eq("mandatory")));
  EXPECT_THAT(config_str->FindString("scope"), Pointee(Eq("user")));
  EXPECT_THAT(config_str->FindString("source"), Pointee(Eq("cloud")));

  const base::DictValue* config_int = config_entries->FindDict("config_int");
  ASSERT_TRUE(config_int);
  EXPECT_EQ(config_int->FindInt("value"), 42);

  base::DictValue names = provider->GetNames();
  VerifyProviderNames(names, scope_key, kTestIwaAppName,
                      {"config_str", "config_int"});
}

TEST_F(WebAppManagedConfigurationValueProviderTest,
       GetValues_ReturnsPwaManagedConfiguration) {
  constexpr char kTestPwaAppName[] = "Test Progressive Web App";
  const GURL pwa_url("https://example.com/app/index.html");
  const std::string pwa_scope = "https://example.com/app/";
  const url::Origin pwa_origin = url::Origin::Create(pwa_url);
  const std::string serialized_origin = pwa_origin.Serialize();

  web_app::test::InstallDummyWebApp(profile_.get(), kTestPwaAppName, pwa_url);
  SetManagedConfigurationPref(pwa_origin);

  auto provider =
      WebAppManagedConfigurationValueProvider::Create(profile_.get());
  ASSERT_TRUE(provider);

  base::DictValue config;
  config.Set("pwa_config_key", "pwa_config_val");
  managed_config_api()->SetConfigurationForTesting(pwa_origin,
                                                   std::move(config));
  task_environment_.RunUntilIdle();

  base::DictValue values = provider->GetValues();
  const base::DictValue* pwa_table = values.FindDict(pwa_scope);
  ASSERT_TRUE(pwa_table);
  EXPECT_THAT(pwa_table->FindString(kNameKey), Pointee(Eq(kTestPwaAppName)));
  EXPECT_THAT(pwa_table->FindString(kIdKey), Pointee(Eq(serialized_origin)));
  EXPECT_EQ(pwa_table->FindBool("isExtension"), false);
  EXPECT_EQ(pwa_table->FindBool("isWebApp"), true);

  const base::DictValue* config_entries = pwa_table->FindDict(kPoliciesKey);
  ASSERT_TRUE(config_entries);
  const base::DictValue* pwa_config =
      config_entries->FindDict("pwa_config_key");
  ASSERT_TRUE(pwa_config);
  EXPECT_THAT(pwa_config->FindString("value"), Pointee(Eq("pwa_config_val")));

  VerifyProviderNames(provider->GetNames(), pwa_scope, kTestPwaAppName,
                      {"pwa_config_key"});
}

TEST_F(WebAppManagedConfigurationValueProviderTest,
       GetValues_ReturnsMultipleSectionsForMultiplePwasOnSameOrigin) {
  const GURL docs_url("https://docs.example.com/document/index.html");
  const GURL sheets_url("https://docs.example.com/spreadsheets/index.html");
  const std::string docs_scope = "https://docs.example.com/document/";
  const std::string sheets_scope = "https://docs.example.com/spreadsheets/";
  const url::Origin shared_origin = url::Origin::Create(docs_url);
  const std::string serialized_origin = shared_origin.Serialize();

  web_app::test::InstallDummyWebApp(profile_.get(), "Docs App", docs_url);
  web_app::test::InstallDummyWebApp(profile_.get(), "Sheets App", sheets_url);
  SetManagedConfigurationPref(shared_origin);

  auto provider =
      WebAppManagedConfigurationValueProvider::Create(profile_.get());
  ASSERT_TRUE(provider);

  base::DictValue config;
  config.Set("shared_config_key", "shared_config_val");
  managed_config_api()->SetConfigurationForTesting(shared_origin,
                                                   std::move(config));
  task_environment_.RunUntilIdle();

  base::DictValue values = provider->GetValues();

  const base::DictValue* docs_table = values.FindDict(docs_scope);
  ASSERT_TRUE(docs_table);
  EXPECT_THAT(docs_table->FindString(kNameKey), Pointee(Eq("Docs App")));
  EXPECT_THAT(docs_table->FindString(kIdKey), Pointee(Eq(serialized_origin)));

  const base::DictValue* sheets_table = values.FindDict(sheets_scope);
  ASSERT_TRUE(sheets_table);
  EXPECT_THAT(sheets_table->FindString(kNameKey), Pointee(Eq("Sheets App")));
  EXPECT_THAT(sheets_table->FindString(kIdKey), Pointee(Eq(serialized_origin)));

  base::DictValue names = provider->GetNames();
  VerifyProviderNames(names, docs_scope, "Docs App", {"shared_config_key"});
  VerifyProviderNames(names, sheets_scope, "Sheets App", {"shared_config_key"});
}

TEST_F(WebAppManagedConfigurationValueProviderTest,
       GetValues_ReturnsUninstalledOriginManagedConfiguration) {
  const url::Origin uninstalled_origin =
      url::Origin::Create(GURL("https://uninstalled-app.example.com"));
  const std::string serialized_origin = uninstalled_origin.Serialize();

  SetManagedConfigurationPref(uninstalled_origin);

  auto provider =
      WebAppManagedConfigurationValueProvider::Create(profile_.get());
  ASSERT_TRUE(provider);

  base::DictValue config;
  config.Set("uninstalled_config_key", true);
  managed_config_api()->SetConfigurationForTesting(uninstalled_origin,
                                                   std::move(config));
  task_environment_.RunUntilIdle();

  base::DictValue values = provider->GetValues();
  const base::DictValue* origin_table = values.FindDict(serialized_origin);
  ASSERT_TRUE(origin_table);
  EXPECT_THAT(origin_table->FindString(kNameKey), Pointee(Eq("")));
  EXPECT_THAT(origin_table->FindString(kIdKey), Pointee(Eq(serialized_origin)));
  EXPECT_EQ(origin_table->FindBool("isExtension"), false);
  EXPECT_EQ(origin_table->FindBool("isWebApp"), true);

  VerifyProviderNames(provider->GetNames(), serialized_origin, "",
                      {"uninstalled_config_key"});
}

TEST_F(WebAppManagedConfigurationValueProviderTest,
       GetValues_DynamicallyUpdatedOnUninstall) {
  web_app::IsolatedWebAppUrlInfo url_info = InstallTestIwa();
  const std::string origin_id = url_info.origin().Serialize();
  const std::string scope_key = url_info.origin().GetURL().spec();
  SetManagedConfigurationPref(url_info.origin());

  auto provider =
      WebAppManagedConfigurationValueProvider::Create(profile_.get());
  ASSERT_TRUE(provider);

  base::DictValue config;
  config.Set("config_key1", "config_val1");
  managed_config_api()->SetConfigurationForTesting(url_info.origin(),
                                                   std::move(config));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(provider->GetValues().FindDict(scope_key));
  EXPECT_THAT(provider->GetValues().FindDict(scope_key)->FindString(kNameKey),
              Pointee(Eq(kTestIwaAppName)));

  auto* web_app_provider = web_app::WebAppProvider::GetForTest(profile_.get());
  {
    web_app::ScopedRegistryUpdate update =
        web_app_provider->sync_bridge_unsafe().BeginUpdate();
    update->DeleteApp(url_info.app_id());
  }
  web_app_provider->install_manager().NotifyWebAppUninstalled(
      url_info.app_id(), webapps::WebappUninstallSource::kAppMenu);
  task_environment_.RunUntilIdle();

  base::DictValue values = provider->GetValues();
  const base::DictValue* uninstalled_table = values.FindDict(origin_id);
  ASSERT_TRUE(uninstalled_table);
  EXPECT_THAT(uninstalled_table->FindString(kNameKey), Pointee(Eq("")));
  VerifyProviderNames(provider->GetNames(), origin_id, "", {"config_key1"});
}

TEST_F(WebAppManagedConfigurationValueProviderTest,
       ObserverNotifiedOnManagedConfigurationChange) {
  web_app::IsolatedWebAppUrlInfo url_info = InstallTestIwa();
  SetManagedConfigurationPref(url_info.origin());

  auto provider =
      WebAppManagedConfigurationValueProvider::Create(profile_.get());
  ASSERT_TRUE(provider);

  TestPolicyValueProviderObserver observer;
  provider->AddObserver(&observer);

  base::DictValue config;
  config.Set("config_key1", "config_val1");
  managed_config_api()->SetConfigurationForTesting(url_info.origin(),
                                                   std::move(config));
  task_environment_.RunUntilIdle();

  EXPECT_GE(observer.policy_values_changed_count(), 1);

  provider->RemoveObserver(&observer);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

}  // namespace policy
