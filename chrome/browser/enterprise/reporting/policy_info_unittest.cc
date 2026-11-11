// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/policy_info.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_merger.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {
constexpr char kPolicyName1[] = "policy_a";
constexpr char kPolicyName2[] = "policy_b";

constexpr char kExtensionId1[] = "abcdefghijklmnoabcdefghijklmnoab";
constexpr char kExtensionId2[] = "abcdefghijklmnoabcdefghijklmnoac";
}  // namespace

using ::testing::_;
using ::testing::Eq;

class PolicyInfoTest : public ::testing::Test {
 public:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    std::string test_profile_name = "test_profile";
    profile_ = profile_manager_->CreateTestingProfile(
        test_profile_name,
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(test_profile_name), 0,
        TestingProfile::TestingFactories(), /*is_supervised_profile=*/false,
        std::optional<bool>(), GetPolicyService());
    profile_manager_->CreateTestingProfile(chrome::kInitialProfile);
  }

  std::unique_ptr<policy::MockPolicyService> GetPolicyService() {
    auto policy_service = std::make_unique<policy::MockPolicyService>();
    ON_CALL(*policy_service.get(),
            GetPolicies(Eq(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                   std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));
    ON_CALL(*policy_service.get(),
            GetPolicies(Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_EXTENSIONS, kExtensionId1))))
        .WillByDefault(::testing::ReturnRef(extension_policy_map_));
    ON_CALL(*policy_service.get(),
            GetPolicies(Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_EXTENSIONS, kExtensionId2))))
        .WillByDefault(::testing::ReturnRef(empty_policy_map_));

    policy_service_ = policy_service.get();
    return policy_service;
  }

  TestingProfile* profile() { return profile_; }
  policy::PolicyMap* policy_map() { return &policy_map_; }
  policy::PolicyMap* extension_policy_map() { return &extension_policy_map_; }
  policy::MockPolicyService* policy_service() { return policy_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  policy::PolicyMap policy_map_;
  policy::PolicyMap extension_policy_map_;
  policy::PolicyMap empty_policy_map_;
  raw_ptr<policy::MockPolicyService> policy_service_;
};

// Verify two Chrome policies are appended to the Profile report properly.
TEST_F(PolicyInfoTest, ChromePolicy) {
  policy_map()->Set(kPolicyName1, policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    base::Value(base::ListValue()), nullptr);
  policy_map()->Set(kPolicyName2, policy::POLICY_LEVEL_RECOMMENDED,
                    policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_MERGED,
                    base::Value(true), nullptr);
  em::ChromeUserProfileInfo profile_info;

  EXPECT_CALL(*policy_service(), GetPolicies(_));

  AppendChromePolicyInfoIntoProfileReport(
      policy::PolicyConversions(
          std::make_unique<policy::ChromePolicyConversionsClient>(profile()))
          .EnableConvertTypes(false)
          .EnablePrettyPrint(false)
          .ToValueDict(),
      &profile_info);
  EXPECT_EQ(2, profile_info.chrome_policies_size());

  auto policy1 = profile_info.chrome_policies(0);
  EXPECT_EQ(kPolicyName1, policy1.name());
  EXPECT_EQ("[]", policy1.value());
  EXPECT_EQ(em::Policy_PolicyLevel_LEVEL_MANDATORY, policy1.level());
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_USER, policy1.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_CLOUD, policy1.source());
  EXPECT_NE("", policy1.error());
  EXPECT_FALSE(policy1.has_conflict_type());

  auto policy2 = profile_info.chrome_policies(1);
  EXPECT_EQ(kPolicyName2, policy2.name());
  EXPECT_EQ("true", policy2.value());
  EXPECT_EQ(em::Policy_PolicyLevel_LEVEL_RECOMMENDED, policy2.level());
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE, policy2.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_MERGED, policy2.source());
  EXPECT_NE("", policy2.error());
  EXPECT_FALSE(policy2.has_conflict_type());
}

TEST_F(PolicyInfoTest, ConflictPolicy) {
  policy::PolicyMap::Entry policy_entry(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_CLOUD, base::Value(true),
      /*external_data_fetcher=*/nullptr);

  policy_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
       policy::POLICY_SOURCE_CLOUD, base::Value(false),
       /*external_data_fetcher=*/nullptr});
  policy_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
       policy::POLICY_SOURCE_PLATFORM, base::Value(true),
       /*external_data_fetcher=*/nullptr});

  policy_map()->Set(kPolicyName1, std::move(policy_entry));

  em::ChromeUserProfileInfo profile_info;

  EXPECT_CALL(*policy_service(), GetPolicies(_));

  AppendChromePolicyInfoIntoProfileReport(
      policy::PolicyConversions(
          std::make_unique<policy::ChromePolicyConversionsClient>(profile()))
          .EnableConvertTypes(false)
          .EnablePrettyPrint(false)
          .ToValueDict(),
      &profile_info);

  auto policy_info = profile_info.chrome_policies(0);
  EXPECT_FALSE(policy_info.has_conflict_type());

  ASSERT_EQ(2, policy_info.conflicts_size());
  auto conflict1 = policy_info.conflicts(0);
  EXPECT_EQ(em::Policy_PolicyLevel_LEVEL_MANDATORY, conflict1.level());
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_USER, conflict1.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_CLOUD, conflict1.source());
  EXPECT_EQ(em::Policy_ConflictType_CONFLICT, conflict1.conflict_type());

  auto conflict2 = policy_info.conflicts(1);
  EXPECT_EQ(em::Policy_PolicyLevel_LEVEL_MANDATORY, conflict2.level());
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE, conflict2.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_PLATFORM, conflict2.source());
  EXPECT_EQ(em::Policy_ConflictType_SUPERSEDED, conflict2.conflict_type());
}

TEST_F(PolicyInfoTest, MergedPolicyConflicts) {
  base::ListValue subset_list;
  subset_list.Append("a");
  base::ListValue full_list;
  full_list.Append("a");
  full_list.Append("b");
  base::ListValue unmerged_diff_list;
  unmerged_diff_list.Append("c");

  // Merged source with a strict subset -> MERGED.
  policy::PolicyMap::Entry policy_entry(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_CLOUD, base::Value(std::move(subset_list)),
      /*external_data_fetcher=*/nullptr);
  // Merged source matching final merged list -> SUPERSEDED.
  policy_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
       policy::POLICY_SOURCE_PLATFORM, base::Value(full_list.Clone()),
       /*external_data_fetcher=*/nullptr});
  // Unmerged source (user cloud without user cloud merge) with differing value
  // -> CONFLICT.
  policy_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
       policy::POLICY_SOURCE_CLOUD, base::Value(std::move(unmerged_diff_list)),
       /*external_data_fetcher=*/nullptr});
  // Unmerged source (recommended level) matching final merged list ->
  // SUPERSEDED.
  policy_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_RECOMMENDED, policy::POLICY_SCOPE_MACHINE,
       policy::POLICY_SOURCE_COMMAND_LINE, base::Value(std::move(full_list)),
       /*external_data_fetcher=*/nullptr});
  // Eligible merge source (mandatory machine command line) with wrong value
  // type -> CONFLICT.
  policy_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
       policy::POLICY_SOURCE_COMMAND_LINE, base::Value("invalid string value"),
       /*external_data_fetcher=*/nullptr});
  policy_map()->Set(kPolicyName1, std::move(policy_entry));

  policy::PolicyListMerger merger({kPolicyName1});
  policy_map()->MergeValues({&merger});

  em::ChromeUserProfileInfo profile_info;
  EXPECT_CALL(*policy_service(), GetPolicies(_));

  AppendChromePolicyInfoIntoProfileReport(
      policy::PolicyConversions(
          std::make_unique<policy::ChromePolicyConversionsClient>(profile()))
          .EnableConvertTypes(false)
          .EnablePrettyPrint(false)
          .ToValueDict(),
      &profile_info);

  ASSERT_EQ(1, profile_info.chrome_policies_size());
  const auto& policy_info = profile_info.chrome_policies(0);
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_MERGED, policy_info.source());
  EXPECT_EQ("[\"a\",\"b\"]", policy_info.value());
  EXPECT_FALSE(policy_info.has_conflict_type());

  ASSERT_EQ(5, policy_info.conflicts_size());
  const auto& unmerged_conflict = policy_info.conflicts(0);
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_USER, unmerged_conflict.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_CLOUD, unmerged_conflict.source());
  EXPECT_EQ(em::Policy_ConflictType_CONFLICT,
            unmerged_conflict.conflict_type());

  const auto& wrong_type_conflict = policy_info.conflicts(1);
  EXPECT_EQ(em::Policy_PolicyLevel_LEVEL_MANDATORY,
            wrong_type_conflict.level());
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE, wrong_type_conflict.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_COMMAND_LINE,
            wrong_type_conflict.source());
  EXPECT_EQ(em::Policy_ConflictType_CONFLICT,
            wrong_type_conflict.conflict_type());

  const auto& merged_conflict = policy_info.conflicts(2);
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE, merged_conflict.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_CLOUD, merged_conflict.source());
  EXPECT_EQ(em::Policy_ConflictType_MERGED, merged_conflict.conflict_type());

  const auto& merged_superseded = policy_info.conflicts(3);
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE, merged_superseded.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_PLATFORM,
            merged_superseded.source());
  EXPECT_EQ(em::Policy_ConflictType_SUPERSEDED,
            merged_superseded.conflict_type());

  const auto& unmerged_superseded = policy_info.conflicts(4);
  EXPECT_EQ(em::Policy_PolicyLevel_LEVEL_RECOMMENDED,
            unmerged_superseded.level());
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE, unmerged_superseded.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_COMMAND_LINE,
            unmerged_superseded.source());
  EXPECT_EQ(em::Policy_ConflictType_SUPERSEDED,
            unmerged_superseded.conflict_type());
}

TEST_F(PolicyInfoTest, ConflictPolicyWithHiddenMachineValues) {
  policy::PolicyMap::Entry policy_entry(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_PLATFORM, base::Value("secret_1"),
      /*external_data_fetcher=*/nullptr);

  // Different underlying machine value: both mask to "********", so string
  // comparison after masking would be wrong; unmasked ConflictType is Override.
  policy_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
       policy::POLICY_SOURCE_CLOUD, base::Value("secret_2"),
       /*external_data_fetcher=*/nullptr});
  // Matching underlying user value: primary masks to "********", but unmasked
  // ConflictType is Supersede.
  policy_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
       policy::POLICY_SOURCE_CLOUD, base::Value("secret_1"),
       /*external_data_fetcher=*/nullptr});
  // Matching underlying machine value: unmasked ConflictType is Supersede.
  policy_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
       policy::POLICY_SOURCE_COMMAND_LINE, base::Value("secret_1"),
       /*external_data_fetcher=*/nullptr});

  policy_map()->Set(kPolicyName1, std::move(policy_entry));

  em::ChromeUserProfileInfo profile_info;
  EXPECT_CALL(*policy_service(), GetPolicies(_));

  auto client =
      std::make_unique<policy::ChromePolicyConversionsClient>(profile());
  client->EnableShowMachineValues(false);

  AppendChromePolicyInfoIntoProfileReport(
      policy::PolicyConversions(std::move(client))
          .EnableConvertTypes(false)
          .EnablePrettyPrint(false)
          .ToValueDict(),
      &profile_info);

  ASSERT_EQ(1, profile_info.chrome_policies_size());
  const auto& policy_info = profile_info.chrome_policies(0);
  EXPECT_EQ("\"********\"", policy_info.value());
  EXPECT_FALSE(policy_info.has_conflict_type());

  ASSERT_EQ(3, policy_info.conflicts_size());
  const auto& diff_machine_conflict = policy_info.conflicts(0);
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE,
            diff_machine_conflict.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_CLOUD,
            diff_machine_conflict.source());
  EXPECT_EQ(em::Policy_ConflictType_CONFLICT,
            diff_machine_conflict.conflict_type());

  const auto& same_user_conflict = policy_info.conflicts(1);
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_USER, same_user_conflict.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_CLOUD, same_user_conflict.source());
  EXPECT_EQ(em::Policy_ConflictType_SUPERSEDED,
            same_user_conflict.conflict_type());

  const auto& same_machine_conflict = policy_info.conflicts(2);
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE,
            same_machine_conflict.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_COMMAND_LINE,
            same_machine_conflict.source());
  EXPECT_EQ(em::Policy_ConflictType_SUPERSEDED,
            same_machine_conflict.conflict_type());
}

#if !BUILDFLAG(IS_ANDROID)
// Verify that extension policies are correctly processed and included in
// enterprise reporting.
TEST_F(PolicyInfoTest, ExtensionPolicy) {
  EXPECT_CALL(*policy_service(), GetPolicies(_)).Times(3);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile());

  extension_registry->AddEnabled(
      extensions::ExtensionBuilder("extension_name")
          .SetID(kExtensionId1)
          .SetManifestPath("storage.managed_schema", "schema.json")
          .Build());
  extension_registry->AddEnabled(
      extensions::ExtensionBuilder("extension_name")
          .SetID(kExtensionId2)
          .SetManifestPath("storage.managed_schema", "schema.json")
          .Build());

  // Set a policy for the first extension only.
  extension_policy_map()->Set(kPolicyName1, policy::POLICY_LEVEL_MANDATORY,
                              policy::POLICY_SCOPE_MACHINE,
                              policy::POLICY_SOURCE_PLATFORM, base::Value(3),
                              nullptr);
  em::ChromeUserProfileInfo profile_info;
  AppendExtensionPolicyInfoIntoProfileReport(
      policy::PolicyConversions(
          std::make_unique<policy::ChromePolicyConversionsClient>(profile()))
          .EnableConvertTypes(false)
          .EnablePrettyPrint(false)
          .ToValueDict(),
      &profile_info);

  // Verify that only the first extension appears in the report.
  // The second extension is not in the report because it has no policy.
  EXPECT_EQ(1, profile_info.extension_policies_size());
  EXPECT_EQ(kExtensionId1, profile_info.extension_policies(0).extension_id());
  EXPECT_EQ(1, profile_info.extension_policies(0).policies_size());

  // Verify the policy details are correctly reported.
  auto policy1 = profile_info.extension_policies(0).policies(0);
  EXPECT_EQ(kPolicyName1, policy1.name());
  EXPECT_EQ("3", policy1.value());
  EXPECT_EQ(em::Policy_PolicyLevel_LEVEL_MANDATORY, policy1.level());
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE, policy1.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_PLATFORM, policy1.source());
  EXPECT_FALSE(policy1.has_conflict_type());

  // Extension policies should show standard error message for unknown policies.
  EXPECT_EQ("Unknown policy.", policy1.error());
}

TEST_F(PolicyInfoTest, ExtensionConflictPolicy) {
  EXPECT_CALL(*policy_service(), GetPolicies(_)).Times(2);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile());

  extension_registry->AddEnabled(
      extensions::ExtensionBuilder("extension_name")
          .SetID(kExtensionId1)
          .SetManifestPath("storage.managed_schema", "schema.json")
          .Build());

  // Build a 3-source conflict without cloning so nested ConflictType
  // recomputation in AddConflictingPolicy is directly exercised.
  policy::PolicyMap::Entry middle_entry(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_CLOUD, base::Value(2),
      /*external_data_fetcher=*/nullptr);
  middle_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
       policy::POLICY_SOURCE_CLOUD, base::Value(1),
       /*external_data_fetcher=*/nullptr});
  middle_entry.AddConflictingPolicy(
      {policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
       policy::POLICY_SOURCE_COMMAND_LINE, base::Value(2),
       /*external_data_fetcher=*/nullptr});

  policy::PolicyMap::Entry applied_entry(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_PLATFORM, base::Value(1),
      /*external_data_fetcher=*/nullptr);
  applied_entry.AddConflictingPolicy(std::move(middle_entry));

  extension_policy_map()->Set(kPolicyName1, std::move(applied_entry));

  em::ChromeUserProfileInfo profile_info;
  AppendExtensionPolicyInfoIntoProfileReport(
      policy::PolicyConversions(
          std::make_unique<policy::ChromePolicyConversionsClient>(profile()))
          .EnableConvertTypes(false)
          .EnablePrettyPrint(false)
          .ToValueDict(),
      &profile_info);

  ASSERT_EQ(1, profile_info.extension_policies_size());
  ASSERT_EQ(1, profile_info.extension_policies(0).policies_size());
  const auto& policy_info = profile_info.extension_policies(0).policies(0);
  EXPECT_EQ("1", policy_info.value());
  EXPECT_FALSE(policy_info.has_conflict_type());

  ASSERT_EQ(3, policy_info.conflicts_size());
  const auto& conflict1 = policy_info.conflicts(0);
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE, conflict1.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_COMMAND_LINE, conflict1.source());
  EXPECT_EQ(em::Policy_ConflictType_CONFLICT, conflict1.conflict_type());

  const auto& conflict2 = policy_info.conflicts(1);
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_MACHINE, conflict2.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_CLOUD, conflict2.source());
  EXPECT_EQ(em::Policy_ConflictType_CONFLICT, conflict2.conflict_type());

  const auto& conflict3 = policy_info.conflicts(2);
  EXPECT_EQ(em::Policy_PolicyScope_SCOPE_USER, conflict3.scope());
  EXPECT_EQ(em::Policy_PolicySource_SOURCE_CLOUD, conflict3.source());
  EXPECT_EQ(em::Policy_ConflictType_SUPERSEDED, conflict3.conflict_type());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(PolicyInfoTest, MachineLevelUserCloudPolicyFetchTimestamp) {
  em::ChromeUserProfileInfo profile_info;
#if !BUILDFLAG(IS_CHROMEOS)
  AppendCloudPolicyFetchTimestamp(
      &profile_info, g_browser_process->browser_policy_connector()
                         ->machine_level_user_cloud_policy_manager());
#endif  // !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(0, profile_info.policy_fetched_timestamps_size());
}

}  // namespace enterprise_reporting
