// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/child_user_interactive_base_test.h"

#include <memory>
#include <string>

#include "base/json/json_writer.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_test_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_test_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_policy_builder.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"

namespace {
constexpr char kEduCoexistenceToSVersion[] = "333024512";
constexpr char kFlagsUrl[] = "chrome://flags";
}  // namespace

namespace ash {

void SetUpSupervisedUserPolicies(
    enterprise_management::CloudPolicySettings* policy_payload) {
  policy_payload->mutable_developertoolsavailability()->set_value(
      static_cast<int64_t>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed));
  policy_payload->mutable_educoexistencetosversion()->set_value(
      kEduCoexistenceToSVersion);
  policy_payload->mutable_lacrossecondaryprofilesallowed()->set_value(false);

  std::string parent_access_config;
  base::JSONWriter::Write(parent_access::PolicyFromConfigs(
                              parent_access::GetDefaultTestConfig(),
                              parent_access::GetDefaultTestConfig(), {}),
                          &parent_access_config);
  policy_payload->mutable_parentaccesscodeconfig()->set_value(
      parent_access_config);

  app_time::AppTimeLimitsPolicyBuilder time_limits_policy;
  time_limits_policy.SetResetTime(6, 0);
  std::string time_limits_policy_value;
  base::JSONWriter::Write(time_limits_policy.value(),
                          &time_limits_policy_value);
  policy_payload->mutable_perapptimelimits()->set_value(
      time_limits_policy_value);

  app_time::AppTimeLimitsAllowlistPolicyBuilder allowlist_policy;
  allowlist_policy.SetUp();
  std::string allowlist_policy_value;
  base::JSONWriter::Write(allowlist_policy.dict(), &allowlist_policy_value);
  policy_payload->mutable_perapptimelimitsallowlist()->set_value(
      allowlist_policy_value);

  policy_payload->mutable_reportarcstatusenabled()->set_value(true);
  policy_payload->mutable_urlblocklist()->mutable_value()->add_entries(
      kFlagsUrl);
}

ChildUserInteractiveBaseTest::ChildUserInteractiveBaseTest() {
  // The InteractiveAshTest constructor does not launch a browser in its
  // constructor, but it is needed here to use the LoggedInUserMixin.
  set_launch_browser_for_testing(
      std::make_unique<full_restore::ScopedLaunchBrowserForTesting>());
}

void ChildUserInteractiveBaseTest::SetUpOnMainThread() {
  InteractiveAshTest::SetUpOnMainThread();
  SetupContextWidget();
  SetUpSupervisedUserPolicies(logged_in_user_mixin_.GetUserPolicyMixin()
                                  ->RequestPolicyUpdate()
                                  ->policy_payload());
  logged_in_user_mixin_.LogInUser();
}

}  // namespace ash
