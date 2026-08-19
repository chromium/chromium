// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/isolate_origins_policy_handler.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"

#include "base/byte_size.h"
#include "base/system/sys_info.h"

#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/site_isolation/features.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

#if BUILDFLAG(IS_ANDROID)
// The memory threshold separating high-end Android devices (which apply the
// standard IsolateOrigins policy) from resource-constrained Android devices
// (which apply the IsolateOriginsShortlist policy).
constexpr uint64_t kLowMemoryDeviceThresholdMiB = 3200;

// Helper to check if a feature is enabled during early startup.
// In the production browser, base::FeatureList is initialized before this
// handler runs, so we use the standard IsEnabled() API. In environments where
// the feature list isn't initialized (such as early startup and browser tests),
// we fall back to parsing the raw command line.
bool IsFeatureEnabled(const base::Feature& feature) {
  if (base::FeatureList::GetInstance()) {
    return base::FeatureList::IsEnabled(feature);
  }

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableFeatures)) {
    std::vector<std::string> disabled = base::SplitString(
        command_line->GetSwitchValueASCII(switches::kDisableFeatures), ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (std::ranges::contains(disabled, feature.name)) {
      return false;
    }
  }
  if (command_line->HasSwitch(switches::kEnableFeatures)) {
    std::vector<std::string> enabled = base::SplitString(
        command_line->GetSwitchValueASCII(switches::kEnableFeatures), ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (std::ranges::contains(enabled, feature.name)) {
      return true;
    }
  }

  return feature.default_state == base::FEATURE_ENABLED_BY_DEFAULT;
}
#endif  // BUILDFLAG(IS_ANDROID)

bool ValidatePolicyType(const PolicyMap& policies,
                        const char* policy_name,
                        base::Value::Type expected_type,
                        PolicyErrorMap* errors) {
  const base::Value* value = policies.GetValueUnsafe(policy_name);
  if (value && value->type() != expected_type) {
    if (errors) {
      errors->AddError(policy_name, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(expected_type));
    }
    return false;
  }
  return true;
}

}  // namespace

IsolateOriginsPolicyHandler::IsolateOriginsPolicyHandler() = default;
IsolateOriginsPolicyHandler::~IsolateOriginsPolicyHandler() = default;

bool IsolateOriginsPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                      PolicyErrorMap* errors) {
  std::vector<const char*> policies_to_check;

#if BUILDFLAG(IS_ANDROID)
  policies_to_check.push_back(key::kIsolateOriginsAndroid);
  if (IsFeatureEnabled(site_isolation::features::kIsolateOriginsShortlist)) {
    policies_to_check.push_back(key::kIsolateOrigins);
    policies_to_check.push_back(key::kIsolateOriginsShortlist);
  }
#else
  policies_to_check.push_back(key::kIsolateOrigins);
#endif

  for (const char* policy_name : policies_to_check) {
    if (!ValidatePolicyType(policies, policy_name,
                            base::Value::Type::STRING, errors)) {
      return false;
    }
  }

  return true;
}

void IsolateOriginsPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  const base::Value* value_to_use = nullptr;

#if BUILDFLAG(IS_ANDROID)
  if (IsFeatureEnabled(site_isolation::features::kIsolateOriginsShortlist)) {
    if (base::SysInfo::AmountOfTotalPhysicalMemory() <=
        base::MiB(kLowMemoryDeviceThresholdMiB)) {
      value_to_use = policies.GetValue(key::kIsolateOriginsShortlist,
                                       base::Value::Type::STRING);
    } else {
      value_to_use = policies.GetValue(key::kIsolateOrigins,
                                       base::Value::Type::STRING);
    }

    if (!value_to_use) {
      value_to_use = policies.GetValue(key::kIsolateOriginsAndroid,
                                       base::Value::Type::STRING);
    }
  } else {
    value_to_use = policies.GetValue(key::kIsolateOriginsAndroid,
                                     base::Value::Type::STRING);
  }
#else
  value_to_use =
      policies.GetValue(key::kIsolateOrigins, base::Value::Type::STRING);
#endif

  if (value_to_use) {
    prefs->SetString(prefs::kIsolateOrigins, value_to_use->GetString());
  }
}

}  // namespace policy
