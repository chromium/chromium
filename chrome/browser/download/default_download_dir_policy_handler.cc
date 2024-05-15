// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/default_download_dir_policy_handler.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

DefaultDownloadDirPolicyHandler::DefaultDownloadDirPolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kDefaultDownloadDirectory,
                                base::Value::Type::STRING) {}

DefaultDownloadDirPolicyHandler::~DefaultDownloadDirPolicyHandler() = default;

bool DefaultDownloadDirPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;
  return true;
}

void DefaultDownloadDirPolicyHandler::ApplyPolicySettingsWithParameters(
    const policy::PolicyMap& policies,
    const policy::PolicyHandlerParameters& parameters,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::STRING);
  if (!value)
    return;
  std::string str_value = value->GetString();
  base::FilePath::StringType string_value =
#if BUILDFLAG(IS_WIN)
      base::UTF8ToWide(str_value);
#else
      str_value;
#endif

  base::FilePath::StringType expanded_value =
      download_dir_util::ExpandDownloadDirectoryPath(string_value, parameters);
  bool has_valid_download_dir_policy =
      policies.GetValue(policy::key::kDownloadDirectory,
                        base::Value::Type::STRING) != nullptr;
  if (policies.Get(policy_name())->level == policy::POLICY_LEVEL_RECOMMENDED) {
#if BUILDFLAG(IS_WIN)
    if (!has_valid_download_dir_policy) {
      prefs->SetValue(prefs::kDownloadDefaultDirectory,
                      base::Value(base::WideToUTF8(expanded_value)));
    }
    prefs->SetValue(prefs::kSaveFileDefaultDirectory,
                    base::Value(base::WideToUTF8(expanded_value)));
#else
    if (!has_valid_download_dir_policy) {
      prefs->SetValue(prefs::kDownloadDefaultDirectory,
                      base::Value(expanded_value));
    }
    prefs->SetValue(prefs::kSaveFileDefaultDirectory,
                    base::Value(expanded_value));
#endif
    // Prevents a download path set by policy from being reset because it is
    // dangerous.
    prefs->SetBoolean(prefs::kDownloadDirUpgraded, true);
  }
}

void DefaultDownloadDirPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& /* policies */,
    PrefValueMap* /* prefs */) {
  NOTREACHED_IN_MIGRATION();
}
