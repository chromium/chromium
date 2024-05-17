// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_dir_policy_handler.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/pref_names.h"
#include "components/drive/drive_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

DownloadDirPolicyHandler::DownloadDirPolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kDownloadDirectory,
                                base::Value::Type::STRING) {}

DownloadDirPolicyHandler::~DownloadDirPolicyHandler() {}

bool DownloadDirPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

#if BUILDFLAG(IS_CHROMEOS)
  // Download directory can only be set as a user policy. If it is set through
  // platform policy for a chromeos=1 build, ignore it.
  if (value &&
      policies.Get(policy_name())->scope != policy::POLICY_SCOPE_USER) {
    errors->AddError(policy_name(), IDS_POLICY_SCOPE_ERROR);
    return false;
  }
#endif

  return true;
}

void DownloadDirPolicyHandler::ApplyPolicySettingsWithParameters(
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

  // Make sure the path isn't empty, since that will point to an undefined
  // location; the default location is used instead in that case.
  // This is checked after path expansion because a non-empty policy value can
  // lead to an empty path value after expansion (e.g. "\"\"").
  base::FilePath::StringType expanded_value =
      download_dir_util::ExpandDownloadDirectoryPath(string_value, parameters);
  if (expanded_value.empty()) {
    expanded_value = policy::path_parser::ExpandPathVariables(
        DownloadPrefs::GetDefaultDownloadDirectory().value());
  }
#if BUILDFLAG(IS_WIN)
  prefs->SetValue(prefs::kDownloadDefaultDirectory,
                  base::Value(base::WideToUTF8(expanded_value)));
#else
  prefs->SetValue(prefs::kDownloadDefaultDirectory,
                  base::Value(expanded_value));
#endif

  // If the policy is mandatory, prompt for download should be disabled.
  // Otherwise, it would enable a user to bypass the mandatory policy.
  // Also check if the LocalUserFilesAllowed is set to False, in that case set
  // the pref to control the default folder in the Files App.
  if (policies.Get(policy_name())->level == policy::POLICY_LEVEL_MANDATORY) {
    prefs->SetBoolean(prefs::kPromptForDownload, false);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Drive is disabled only in Ash and not Lacros, because Lacros respects
    // Drive availability status in Ash automatically.
    if (download_dir_util::DownloadToDrive(string_value, parameters)) {
      prefs->SetBoolean(drive::prefs::kDisableDrive, false);

      prefs->SetString(prefs::kFilesAppDefaultLocation,
                       download_dir_util::kLocationGoogleDrive);
    } else if (download_dir_util::DownloadToOneDrive(string_value,
                                                     parameters)) {
      prefs->SetString(prefs::kFilesAppDefaultLocation,
                       download_dir_util::kLocationOneDrive);
      prefs->SetBoolean(prefs::kAllowUserToRemoveODFS, false);
    }

#endif
  }  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void DownloadDirPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& /* policies */,
    PrefValueMap* /* prefs */) {
  NOTREACHED_IN_MIGRATION();
}
