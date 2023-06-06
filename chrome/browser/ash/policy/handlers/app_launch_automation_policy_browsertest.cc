// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/app_launch_automation_policy_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/desks_storage/core/admin_template_service.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
base::Value ParsePolicyFromString(base::StringPiece policy) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(policy);

  CHECK(parsed_json.has_value());
  CHECK(parsed_json->is_list());

  return std::move(parsed_json.value());
}

policy::PolicyMap GetPolicyMap(const base::Value& policy_value) {
  policy::PolicyMap::Entry entry(
      policy::PolicyLevel::POLICY_LEVEL_RECOMMENDED,
      policy::PolicyScope::POLICY_SCOPE_USER,
      policy::PolicySource::POLICY_SOURCE_ENTERPRISE_DEFAULT,
      absl::optional<base::Value>(policy_value.Clone()),
      /*extenal_data_fetcher =*/nullptr);

  policy::PolicyMap map;
  map.Set(policy::key::kAppLaunchAutomation, std::move(entry));
  return map;
}
}  // namespace

class AppLaunchAutomationPolicyTest : public InProcessBrowserTest {
 public:
  AppLaunchAutomationPolicyTest()
      : scoped_feature_list_(ash::features::kAppLaunchAutomation) {}
  AppLaunchAutomationPolicyTest(const AppLaunchAutomationPolicyTest&) = delete;
  AppLaunchAutomationPolicyTest& operator=(
      const AppLaunchAutomationPolicyTest&) = delete;
  ~AppLaunchAutomationPolicyTest() override = default;

  desks_storage::AdminTemplateService* GetAdminService() {
    return ash::Shell::Get()->saved_desk_delegate()->GetAdminTemplateService();
  }

  void WaitForAdminTemplateService() {
    auto* admin_template_service = GetAdminService();
    if (!admin_template_service) {
      return;
    }
    while (!admin_template_service->IsReady()) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  PrefValueMap* GetPrefValueMap() { return &pref_map_; }

  // Returns a policy map containing the correct value.
  policy::PolicyMap GetStandardPolicyMap() {
    return GetPolicyMap(ParsePolicyFromString(
        desks_storage::desk_test_util::kAdminTemplatePolicy));
  }

  policy::PolicyMap GetModifiedPolicyMap() {
    return GetPolicyMap(ParsePolicyFromString(
        desks_storage::desk_test_util::kAdminTemplatePolicyWithOneTemplate));
  }

  policy::PolicyMap GetEmptyPolicyMap() {
    return GetPolicyMap(base::Value(base::Value::List()));
  }

 private:
  PrefValueMap pref_map_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppLaunchAutomationPolicyTest,
                       AppliesPolicySettingCorrectly) {
  WaitForAdminTemplateService();
  policy::AppLaunchAutomationPolicyHandler handler(policy::GetChromeSchema());

  handler.ApplyPolicySettings(GetStandardPolicyMap(), GetPrefValueMap());

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 2UL);
}

IN_PROC_BROWSER_TEST_F(AppLaunchAutomationPolicyTest,
                       AppliesModifiedPolicySettingCorrectly) {
  WaitForAdminTemplateService();
  policy::AppLaunchAutomationPolicyHandler handler(policy::GetChromeSchema());

  handler.ApplyPolicySettings(GetStandardPolicyMap(), GetPrefValueMap());
  handler.ApplyPolicySettings(GetModifiedPolicyMap(), GetPrefValueMap());

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 1UL);
}

IN_PROC_BROWSER_TEST_F(AppLaunchAutomationPolicyTest,
                       AppliesEmptyPolicySettingCorrectly) {
  WaitForAdminTemplateService();
  policy::AppLaunchAutomationPolicyHandler handler(policy::GetChromeSchema());

  handler.ApplyPolicySettings(GetStandardPolicyMap(), GetPrefValueMap());
  handler.ApplyPolicySettings(GetEmptyPolicyMap(), GetPrefValueMap());

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 0UL);
}

IN_PROC_BROWSER_TEST_F(AppLaunchAutomationPolicyTest,
                       AppliesAdditionalPolicySettingCorrectly) {
  WaitForAdminTemplateService();
  policy::AppLaunchAutomationPolicyHandler handler(policy::GetChromeSchema());

  handler.ApplyPolicySettings(GetModifiedPolicyMap(), GetPrefValueMap());
  handler.ApplyPolicySettings(GetStandardPolicyMap(), GetPrefValueMap());

  auto* admin_service = GetAdminService();
  ASSERT_TRUE(admin_service != nullptr);

  EXPECT_EQ(admin_service->GetFullDeskModel()->GetEntryCount(), 2UL);
}
