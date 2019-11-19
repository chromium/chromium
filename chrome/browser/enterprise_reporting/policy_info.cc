// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/policy_info.h"

#include "base/json/json_writer.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/policy_conversions.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_types.h"

namespace enterprise_reporting {

namespace {

em::Policy_PolicyLevel GetLevel(const base::Value& policy) {
  switch (static_cast<policy::PolicyLevel>(*policy.FindIntKey("level"))) {
    case policy::POLICY_LEVEL_RECOMMENDED:
      return em::Policy_PolicyLevel_LEVEL_RECOMMENDED;
    case policy::POLICY_LEVEL_MANDATORY:
      return em::Policy_PolicyLevel_LEVEL_MANDATORY;
  }
  NOTREACHED() << "Invalid policy level: " << *policy.FindIntKey("level");
  return em::Policy_PolicyLevel_LEVEL_UNKNOWN;
}

em::Policy_PolicyScope GetScope(const base::Value& policy) {
  switch (static_cast<policy::PolicyScope>(*policy.FindIntKey("scope"))) {
    case policy::POLICY_SCOPE_USER:
      return em::Policy_PolicyScope_SCOPE_USER;
    case policy::POLICY_SCOPE_MACHINE:
      return em::Policy_PolicyScope_SCOPE_MACHINE;
  }
  NOTREACHED() << "Invalid policy scope: " << *policy.FindIntKey("scope");
  return em::Policy_PolicyScope_SCOPE_UNKNOWN;
}

em::Policy_PolicySource GetSource(const base::Value& policy) {
  switch (static_cast<policy::PolicySource>(*policy.FindIntKey("source"))) {
    case policy::POLICY_SOURCE_ENTERPRISE_DEFAULT:
      return em::Policy_PolicySource_SOURCE_ENTERPRISE_DEFAULT;
    case policy::POLICY_SOURCE_CLOUD:
      return em::Policy_PolicySource_SOURCE_CLOUD;
    case policy::POLICY_SOURCE_ACTIVE_DIRECTORY:
      return em::Policy_PolicySource_SOURCE_ACTIVE_DIRECTORY;
    case policy::POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE:
      return em::Policy_PolicySource_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE;
    case policy::POLICY_SOURCE_PLATFORM:
      return em::Policy_PolicySource_SOURCE_PLATFORM;
    case policy::POLICY_SOURCE_PRIORITY_CLOUD:
      return em::Policy_PolicySource_SOURCE_PRIORITY_CLOUD;
    case policy::POLICY_SOURCE_MERGED:
      return em::Policy_PolicySource_SOURCE_MERGED;
    case policy::POLICY_SOURCE_COUNT:
      NOTREACHED();
      return em::Policy_PolicySource_SOURCE_UNKNOWN;
  }
  NOTREACHED() << "Invalid policy source: " << *policy.FindIntKey("source");
  return em::Policy_PolicySource_SOURCE_UNKNOWN;
}

void UpdatePolicyInfo(em::Policy* policy_info,
                      const std::string& policy_name,
                      const base::Value& policy) {
  policy_info->set_name(policy_name);
  policy_info->set_level(GetLevel(policy));
  policy_info->set_scope(GetScope(policy));
  policy_info->set_source(GetSource(policy));
  base::JSONWriter::Write(*policy.FindKey("value"),
                          policy_info->mutable_value());
  const std::string* error = policy.FindStringKey("error");
  if (error)
    policy_info->set_error(*error);
}

}  // namespace

void AppendChromePolicyInfoIntoProfileReport(
    const base::Value& policies,
    em::ChromeUserProfileInfo* profile_info) {
  for (const auto& policy_iter :
       policies.FindKey("chromePolicies")->DictItems()) {
    UpdatePolicyInfo(profile_info->add_chrome_policies(), policy_iter.first,
                     policy_iter.second);
  }
}

void AppendExtensionPolicyInfoIntoProfileReport(
    const base::Value& policies,
    em::ChromeUserProfileInfo* profile_info) {
  for (const auto& extension_iter :
       policies.FindKey("extensionPolicies")->DictItems()) {
    const base::Value& policies = extension_iter.second;
    if (policies.DictSize() == 0)
      continue;
    auto* extension = profile_info->add_extension_policies();
    extension->set_extension_id(extension_iter.first);
    for (const auto& policy_iter : policies.DictItems()) {
      UpdatePolicyInfo(extension->add_policies(), policy_iter.first,
                       policy_iter.second);
    }
  }
}

void AppendMachineLevelUserCloudPolicyFetchTimestamp(
    em::ChromeUserProfileInfo* profile_info) {
#if !defined(OS_CHROMEOS)
  policy::MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  if (!manager || !manager->IsClientRegistered())
    return;
  auto* timestamp = profile_info->add_policy_fetched_timestamps();
  timestamp->set_type(
      policy::dm_protocol::kChromeMachineLevelExtensionCloudPolicyType);
  timestamp->set_timestamp(
      manager->core()->client()->last_policy_timestamp().ToJavaTime());
#endif
}

}  // namespace enterprise_reporting
