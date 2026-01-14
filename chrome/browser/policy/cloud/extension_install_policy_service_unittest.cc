// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/extension_install_policy_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_client_types.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

constexpr char kExtensionId[] = "extension-id";
constexpr char kExtensionVersion[] = "1.0.0.0";

base::Value GetPolicyValueForAction(
    const std::string& extension_version,
    enterprise_management::ExtensionInstallPolicy::Action action) {
return base::Value(base::Value::Dict().Set(
           extension_version,
           base::Value::Dict().Set("action", action)));
}

}  // namespace

class ExtensionInstallPolicyServiceTest : public testing::Test {
 public:
  void SetUp() override {
    policy_provider_ =
        std::make_unique<testing::NiceMock<MockConfigurationPolicyProvider>>();
    policy_provider_->SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    std::vector<raw_ptr<ConfigurationPolicyProvider, VectorExperimental>>
        providers = {policy_provider_.get()};
    auto policy_service_ = std::make_unique<PolicyServiceImpl>(providers);

    TestingProfile::Builder builder;
    builder.SetPolicyService(std::move(policy_service_));
#if !BUILDFLAG(IS_CHROMEOS)
    builder.SetUserCloudPolicyManager(BuildUserCloudPolicyManager());
#endif  // !BUILDFLAG(IS_CHROMEOS)
    profile_ = builder.Build();

#if !BUILDFLAG(IS_CHROMEOS)
    client_ = std::make_unique<MockCloudPolicyClient>();
    auto* manager = profile_->GetUserCloudPolicyManager();
    CHECK(manager);
    manager->Init(&schema_registry_);
    manager->Connect(g_browser_process->local_state(), std::move(client_));
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }

  void TearDown() override { profile_.reset(); }

  TestingProfile* profile() { return profile_.get(); }

  // Build a test version CloudPolicyManager for testing profiles.
  std::unique_ptr<UserCloudPolicyManager> BuildUserCloudPolicyManager() {
    auto mock_user_cloud_policy_store =
        std::make_unique<MockUserCloudPolicyStore>();
    std::unique_ptr<MockUserCloudPolicyStore>
        mock_user_cloud_policy_extension_install_store;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    mock_user_cloud_policy_extension_install_store =
        std::make_unique<MockUserCloudPolicyStore>();
#endif

    return std::make_unique<UserCloudPolicyManager>(
        std::move(mock_user_cloud_policy_store),
        std::move(mock_user_cloud_policy_extension_install_store),
        base::FilePath(),
        /*cloud_external_data_manager=*/nullptr,
        task_environment_.GetMainThreadTaskRunner(),
        network::TestNetworkConnectionTracker::CreateGetter());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<MockCloudPolicyClient> client_;
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kEnableExtensionInstallPolicyFetching};
  SchemaRegistry schema_registry_;
};

TEST_F(ExtensionInstallPolicyServiceTest, IsExtensionAllowedUnknown) {
  TestingProfile::Builder builder;
  auto policy_service = std::make_unique<MockPolicyService>();
  EXPECT_CALL(*policy_service,
              IsInitializationComplete(POLICY_DOMAIN_EXTENSION_INSTALL))
      .WillRepeatedly(testing::Return(false));
  builder.SetPolicyService(std::move(policy_service));
  auto test_profile = builder.Build();
  ExtensionInstallPolicyServiceImpl service(test_profile.get());
  EXPECT_FALSE(service
                   .IsExtensionAllowed(
                       ExtensionIdAndVersion(kExtensionId, kExtensionVersion))
                   .has_value());
}

TEST_F(ExtensionInstallPolicyServiceTest, IsExtensionAllowedByDefault) {
  ExtensionInstallPolicyServiceImpl service(profile());
  EXPECT_TRUE(service
                  .IsExtensionAllowed(
                      ExtensionIdAndVersion(kExtensionId, kExtensionVersion))
                  .value());
}

TEST_F(ExtensionInstallPolicyServiceTest, IsExtensionAllowedByPolicy) {
  PolicyMap policy;
  policy.Set(kExtensionId, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             GetPolicyValueForAction(
                 kExtensionVersion,
                 enterprise_management::ExtensionInstallPolicy::ACTION_ALLOW),
             nullptr);
  policy_provider_->UpdateExtensionInstallPolicy(policy);

  ExtensionInstallPolicyServiceImpl service(profile());
  EXPECT_TRUE(service
                  .IsExtensionAllowed(
                      ExtensionIdAndVersion(kExtensionId, kExtensionVersion))
                  .value());
}

TEST_F(ExtensionInstallPolicyServiceTest, IsExtensionBlockedByPolicy) {
  PolicyMap policy;
  policy.Set(kExtensionId, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             GetPolicyValueForAction(
                 kExtensionVersion,
                 enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK),
             nullptr);
  policy_provider_->UpdateExtensionInstallPolicy(policy);

  ExtensionInstallPolicyServiceImpl service(profile());
  EXPECT_FALSE(service
                   .IsExtensionAllowed(
                       ExtensionIdAndVersion(kExtensionId, kExtensionVersion))
                   .value());
}

TEST_F(ExtensionInstallPolicyServiceTest,
       IsExtensionBlockedByConflictingPolicy) {
  PolicyMap policy;
  PolicyMap::Entry entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      GetPolicyValueForAction(
          kExtensionVersion,
          enterprise_management::ExtensionInstallPolicy::ACTION_ALLOW),
      nullptr);

  PolicyMap::Entry conflicting_policy(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      GetPolicyValueForAction(
          kExtensionVersion,
          enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK),
      nullptr);
  entry.AddConflictingPolicy(std::move(conflicting_policy));

  policy.Set(kExtensionId, std::move(entry));

  policy_provider_->UpdateExtensionInstallPolicy(policy);

  ExtensionInstallPolicyServiceImpl service(profile());
  EXPECT_FALSE(service
                   .IsExtensionAllowed(
                       ExtensionIdAndVersion(kExtensionId, kExtensionVersion))
                   .value());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ExtensionInstallPolicyServiceTest, TypesToFetch) {
  UserCloudPolicyManager* manager = profile()->GetUserCloudPolicyManager();
  ASSERT_TRUE(manager);

  ASSERT_TRUE(manager->core());
  ASSERT_TRUE(manager->core()->client());

  profile()->GetPrefs()->SetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled, true);
  {
    ExtensionInstallPolicyServiceImpl service(profile());
    // This EIPS should now be in types_to_fetch().
    EXPECT_THAT(manager->core()->client()->types_to_fetch(),
                testing::UnorderedElementsAre(
                    PolicyTypeToFetch(dm_protocol::GetChromeUserPolicyType(),
                                      std::string()),
                    PolicyTypeToFetch(
                        dm_protocol::kChromeExtensionInstallUserCloudPolicyType,
                        &service)));

    // Disable the feature, it should get removed from types_to_fetch().
    profile()->GetPrefs()->SetBoolean(
        extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled,
        false);
    EXPECT_THAT(manager->core()->client()->types_to_fetch(),
                testing::UnorderedElementsAre(PolicyTypeToFetch(
                    dm_protocol::GetChromeUserPolicyType(), std::string())));

    // Re-enable the feature, it should get re-added to types_to_fetch().
    profile()->GetPrefs()->SetBoolean(
        extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled,
        true);
    EXPECT_THAT(manager->core()->client()->types_to_fetch(),
                testing::UnorderedElementsAre(
                    PolicyTypeToFetch(dm_protocol::GetChromeUserPolicyType(),
                                      std::string()),
                    PolicyTypeToFetch(
                        dm_protocol::kChromeExtensionInstallUserCloudPolicyType,
                        &service)));
  }

  EXPECT_THAT(manager->core()->client()->types_to_fetch(),
              testing::UnorderedElementsAre(PolicyTypeToFetch(
                  dm_protocol::GetChromeUserPolicyType(), std::string())));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace policy
