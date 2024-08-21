// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/policy_data_collector.h"

#include <cstdio>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_value_and_status_aggregator.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/browser/webui/policy_webui_constants.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::IsSubsetOf;

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)

// The set of PII types that can be found in policy status.
const std::set<redaction::PIIType> kExpectedPIITypesInPolicyStatus = {
    redaction::PIIType::kStableIdentifier,
    redaction::PIIType::kStableIdentifier, redaction::PIIType::kGaiaID,
    redaction::PIIType::kEmail};

// The set of pairs with policy status keys which are considered as PII. These
// are the common keys between user and device policy status.
const char* kPolicyStatusFieldsWithPII[] = {policy::kClientIdKey,
                                            policy::kEnterpriseDomainManagerKey,
                                            policy::kUsernameKey};

// The set of pairs with policy status keys which don't contain PII. These are
// the common keys between user and device policy status.
const char* kPolicyStatusFields[] = {policy::kPolicyDescriptionKey, "error",
                                     "policiesPushAvailable", "status",
                                     "timeSinceLastRefresh"};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Reads the contents of exported policy Json file in to `policies`
// dictionary.
void ReadExportedPolicyFile(base::Value::Dict* policies,
                            base::FilePath file_path) {
  // Allow blocking for testing in this scope for IO operations.
  base::ScopedAllowBlockingForTesting allow_blocking;
  // `data_collector` will export the output into a file names
  // "policies.json" under `output_path`.
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(file_path, &file_contents));
  std::optional<base::Value> dict_value = base::JSONReader::Read(file_contents);
  ASSERT_TRUE(dict_value);
  *policies = std::move(dict_value->GetDict());
}

class PolicyDataCollectorBrowserTest : public InProcessBrowserTest {
 public:
  PolicyDataCollectorBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    policy::PushProfilePolicyConnectorProviderForTesting(&policy_provider_);

    // Allow blocking for testing in this scope for temporary directory
    // creation.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDownInProcessBrowserTestFixture() override {
    // Allow blocking for testing in this scope for temporary directory
    // creation.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  void AddExpectedChromePolicy(policy::PolicyMap* policy_map,
                               base::Value::Dict* expected_policies,
                               const std::string& policy,
                               policy::PolicyLevel level,
                               const std::string& level_str,
                               policy::PolicyScope scope,
                               const std::string& scope_str,
                               policy::PolicySource source,
                               const std::string& source_str,
                               const std::string& error,
                               base::Value value) {
    policy_map->Set(policy, level, scope, source, value.Clone(),
                    /*external_data_fetcher=*/nullptr);
    base::Value::Dict policy_dict;
    policy_dict.Set("level", level_str);
    policy_dict.Set("scope", scope_str);
    policy_dict.Set("source", source_str);
    policy_dict.Set("value", std::move(value));
    if (!error.empty())
      policy_dict.Set("error", error);
    expected_policies->SetByDottedPath(
        base::StringPrintf("%s.%s", policy::kPoliciesKey, policy.c_str()),
        std::move(policy_dict));
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  // Use a temporary directory to store data collector output.
  base::ScopedTempDir temp_dir_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PolicyDataCollectorBrowserTestAsh
    : public MixinBasedInProcessBrowserTest {
 public:
  PolicyDataCollectorBrowserTestAsh() = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
    // By default DeviceStateMixin sets public key version to 17 whereas policy
    // test server inside LoggedInUserMixin has only one version. By setting
    // public_key_version to 1, we make device policy requests succeed and thus
    // device policy timestamp set.
    device_state_.RequestDevicePolicyUpdate()
        ->policy_data()
        ->set_public_key_version(1);
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    // Allow blocking for testing in this scope for temporary directory
    // creation.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDownInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    // Allow blocking for testing in this scope for temporary directory
    // creation.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  std::set<redaction::PIIType> GetSetOfPIITypesInPIIMap(const PIIMap& pii_map) {
    std::set<redaction::PIIType> pii_types;
    for (const auto& map_entry : pii_map) {
      pii_types.insert(map_entry.first);
    }
    return pii_types;
  }

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      ash::LoggedInUserMixin::LogInType::kManaged};
  // Use a temporary directory to store data collector output.
  base::ScopedTempDir temp_dir_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyDataCollectorBrowserTest,
                       CollectPolicyValuesAndMetadata) {
  // PolicyDataCollector for testing.
  PolicyDataCollector data_collector(browser()->profile());

  // We will use `values` for mocking the policy values `policy_provider_` will
  // return.
  policy::PolicyMap values;

  // Set expected policy values.
  base::Value::Dict expected_policies;
  expected_policies.Set(policy::kNameKey, policy::kChromePoliciesName);
  expected_policies.Set(policy::kPoliciesKey, base::Value::Dict());

  // Add policies for testing.
  base::Value::List popups_blocked_for_urls;
  popups_blocked_for_urls.Append("aaa");
  popups_blocked_for_urls.Append("bbb");
  popups_blocked_for_urls.Append("ccc");
  AddExpectedChromePolicy(
      &values, &expected_policies, policy::key::kPopupsBlockedForUrls,
      policy::POLICY_LEVEL_MANDATORY, "mandatory", policy::POLICY_SCOPE_MACHINE,
      "machine", policy::POLICY_SOURCE_PLATFORM, "platform",
      /*error=*/std::string(), base::Value(std::move(popups_blocked_for_urls)));

  AddExpectedChromePolicy(
      &values, &expected_policies, policy::key::kDefaultImagesSetting,
      policy::POLICY_LEVEL_MANDATORY, "mandatory", policy::POLICY_SCOPE_MACHINE,
      "machine", policy::POLICY_SOURCE_CLOUD, "cloud",
      /*error=*/std::string(), base::Value(2));

  // This also checks that we save unknown policies correctly.
  base::Value::List unknown_policy;
  unknown_policy.Append(true);
  unknown_policy.Append(12);
  const std::string kUnknownPolicy = "NoSuchThing";
  AddExpectedChromePolicy(
      &values, &expected_policies, kUnknownPolicy,
      policy::POLICY_LEVEL_RECOMMENDED, "recommended",
      policy::POLICY_SCOPE_USER, "user", policy::POLICY_SOURCE_CLOUD, "cloud",
      /*error=*/l10n_util::GetStringUTF8(IDS_POLICY_UNKNOWN),
      base::Value(std::move(unknown_policy)));

  // Add the Chrome policies to fake `policy_provider_`.
  policy_provider_.UpdateChromePolicy(values);

  // Collect policies and assert no error returned.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(
      test_future_collect_data.GetCallback(),
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr);
  std::optional<SupportToolError> error = test_future_collect_data.Get();
  EXPECT_EQ(error, std::nullopt);

  // Create a temporary directory to store the output file.
  base::FilePath output_path = temp_dir_.GetPath();
  // Export the collected data into `output_path` and make sure no error is
  // returned.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_export_data;
  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_path,
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr,
      test_future_export_data.GetCallback());
  error = test_future_export_data.Get();
  EXPECT_EQ(error, std::nullopt);

  // The result must contain three main parts: "chromeMetadata", policies and
  // "status".
  base::Value::Dict policy_result;
  ASSERT_NO_FATAL_FAILURE(ReadExportedPolicyFile(
      &policy_result, output_path.Append(FILE_PATH_LITERAL("policies.json"))));

  base::Value::Dict* chrome_metadata = policy_result.FindDict("chromeMetadata");
  ASSERT_TRUE(chrome_metadata);
  // Check that `chrome_metadata` contains all the expected keys.
  // The keys that are expected to be in "chromeMetadata" dictionary are
  // "application", "version", "revision". We don't include platform specific
  // keys because we will test for all platforms.
  EXPECT_TRUE(chrome_metadata->contains("application"));
  EXPECT_TRUE(chrome_metadata->contains("version"));
  EXPECT_TRUE(chrome_metadata->contains("revision"));

  // Check that policy values are the same as expected.
  base::Value::Dict* policy_values =
      policy_result.FindDict(policy::kPolicyValuesKey);
  ASSERT_TRUE(policy_values);
  // We only check Chrome policies as it's common between all platforms. We
  // don't test platform specific policies in this test.
  base::Value::Dict* chrome_policies =
      policy_values->FindDict(policy::kChromePoliciesId);
  ASSERT_TRUE(chrome_policies);
  EXPECT_EQ(*chrome_policies, expected_policies);

  // Check that the returned contains "status". We just check if the returned
  // status is not empty.
  base::Value::Dict* status = policy_result.FindDict("status");
  ASSERT_TRUE(status);
  EXPECT_FALSE(status->empty());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// We test the status in detail for only Ash in
// PolicyDataCollectorBrowserTestAsh.CollectPolicyStatus because the Mixins
// for logged-in user only exists for Ash.
IN_PROC_BROWSER_TEST_F(PolicyDataCollectorBrowserTestAsh, CollectPolicyStatus) {
  // PolicyDataCollector for testing.
  PolicyDataCollector data_collector(ProfileManager::GetActiveUserProfile());

  // Collect policies and assert no error returned.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(
      test_future_collect_data.GetCallback(),
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr);
  std::optional<SupportToolError> error = test_future_collect_data.Get();
  EXPECT_EQ(error, std::nullopt);

  // Check the returned map of detected PII inside the collected data to see if
  // it contains the PII types we expect.
  PIIMap pii_map = data_collector.GetDetectedPII();
  EXPECT_THAT(GetSetOfPIITypesInPIIMap(pii_map),
              IsSubsetOf(kExpectedPIITypesInPolicyStatus));

  // Create a temporary directory to store the output file.
  base::FilePath output_path = temp_dir_.GetPath();
  // Export the collected data into `output_path` and make sure no error is
  // returned.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_export_data;
  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_path,
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr,
      test_future_export_data.GetCallback());
  error = test_future_export_data.Get();
  EXPECT_EQ(error, std::nullopt);

  base::Value::Dict policy_result;
  ASSERT_NO_FATAL_FAILURE(ReadExportedPolicyFile(
      &policy_result, output_path.Append(FILE_PATH_LITERAL("policies.json"))));
  EXPECT_FALSE(policy_result.empty());

  base::Value::Dict* status = policy_result.FindDict("status");
  ASSERT_TRUE(status);

  // Check device policy status.
  base::Value::Dict* device_policy_status =
      status->FindDict(policy::kDeviceStatusKey);
  EXPECT_TRUE(device_policy_status);
  // Check the policy status fields with PII.
  for (const char* device_status_key : kPolicyStatusFieldsWithPII) {
    EXPECT_TRUE(device_policy_status->contains(device_status_key))
        << "Device policy status doesn't contain key: " << device_status_key;
    base::Value* device_status = device_policy_status->Find(device_status_key);
    ASSERT_TRUE(device_status);
    // Check if the fields containing PII are properly masked.
    EXPECT_TRUE(device_status->is_string())
        << "Device policy key " << device_status_key
        << " is not a string as expected";
    EXPECT_EQ(device_status->GetString(), kRedactedPlaceholder);
  }
  // Check the policy status fields without PII.
  for (const char* device_status_key : kPolicyStatusFields) {
    EXPECT_TRUE(device_policy_status->contains(device_status_key))
        << "Device policy status doesn't contain key: " << device_status_key;
    base::Value* device_status = device_policy_status->Find(device_status_key);
    ASSERT_TRUE(device_status);
    // If the key is not PII, the value can either be string or a bool. Check
    // that it's not empty if it's a string.
    if (device_status->is_string()) {
      EXPECT_NE(device_status->GetString(), std::string());
    }
  }

  // Check user policy status.
  base::Value::Dict* user_policy_status =
      status->FindDict(policy::kUserStatusKey);
  EXPECT_TRUE(user_policy_status);
  // Check the policy status fields with PII.
  for (const char* user_status_key : kPolicyStatusFieldsWithPII) {
    EXPECT_TRUE(user_policy_status->contains(user_status_key))
        << "User policy status doesn't contain key: " << user_status_key;
    base::Value* user_status = user_policy_status->Find(user_status_key);
    ASSERT_TRUE(user_status);
    // Check if the fields containing PII are properly masked.
    EXPECT_TRUE(user_status->is_string())
        << "User policy key " << user_status_key
        << " is not a string as expected";
    EXPECT_EQ(user_status->GetString(), kRedactedPlaceholder);
  }
  // Check the policy status fields without PII.
  for (const char* user_status_key : kPolicyStatusFields) {
    EXPECT_TRUE(user_policy_status->contains(user_status_key))
        << "User policy status doesn't contain key: " << user_status_key;
    base::Value* user_status = user_policy_status->Find(user_status_key);
    ASSERT_TRUE(user_status);
    // If the key is not PII, the value can either be string or a bool. Check
    // that it's not empty if it's a string.
    if (user_status->is_string()) {
      EXPECT_NE(user_status->GetString(), std::string());
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
