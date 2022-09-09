// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/disk_cache_dir_policy_handler.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

DiskCacheDirPolicyHandler::DiskCacheDirPolicyHandler()
    : TypeCheckingPolicyHandler(key::kDiskCacheDir, base::Value::Type::STRING) {
}

DiskCacheDirPolicyHandler::~DiskCacheDirPolicyHandler() {}

void DiskCacheDirPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::STRING);
  if (!value)
    return;

  base::FilePath::StringType expanded_value =
#if BUILDFLAG(IS_WIN)
      policy::path_parser::ExpandPathVariables(
          base::UTF8ToWide(value->GetString()));
#else
      policy::path_parser::ExpandPathVariables(value->GetString());
#endif
  base::FilePath expanded_path(expanded_value);
  prefs->SetValue(prefs::kDiskCacheDir,
                  base::Value(expanded_path.AsUTF8Unsafe()));
}

}  // namespace policy
