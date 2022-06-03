// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_auto_open_policy_handler.h"

#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

DownloadAutoOpenPolicyHandler::DownloadAutoOpenPolicyHandler(
    const policy::Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          policy::key::kAutoOpenFileTypes,
          chrome_schema.GetKnownProperty(policy::key::kAutoOpenFileTypes),
          policy::SCHEMA_ALLOW_UNKNOWN) {}

DownloadAutoOpenPolicyHandler::~DownloadAutoOpenPolicyHandler() = default;

bool DownloadAutoOpenPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, nullptr, &policy_value) || !policy_value ||
      !policy_value->is_list())
    return false;

  base::Value::ConstListView policy_list = policy_value->GetList();
  for (size_t i = 0; i < policy_list.size(); ++i) {
    const std::string extension = policy_list[i].GetString();
    // If it's empty or malformed, then mark it as an error.
    if (extension.empty() ||
        *extension.begin() == base::FilePath::kExtensionSeparator) {
      errors->AddError(policy::key::kAutoOpenFileTypes, i,
                       IDS_POLICY_VALUE_FORMAT_ERROR);
    }
  }

  // Always continue to ApplyPolicySettings which can remove invalid values and
  // apply the valid ones.
  return true;
}

void DownloadAutoOpenPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs_value_map) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, nullptr, &policy_value) || !policy_value)
    return;
  DCHECK(policy_value->is_list());

  base::ListValue pref_values;
  for (const auto& entry : policy_value->GetList()) {
    const std::string extension = entry.GetString();
    // If it's empty or malformed, then skip the entry.
    if (extension.empty() ||
        *extension.begin() == base::FilePath::kExtensionSeparator) {
      continue;
    }
    pref_values.Append(extension);
  }
  prefs_value_map->SetValue(prefs::kDownloadExtensionsToOpenByPolicy,
                            std::move(pref_values));
}
