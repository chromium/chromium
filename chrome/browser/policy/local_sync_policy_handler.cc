// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/local_sync_policy_handler.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/sync/base/pref_names.h"

namespace policy {

LocalSyncPolicyHandler::LocalSyncPolicyHandler()
    : TypeCheckingPolicyHandler(key::kRoamingProfileLocation,
                                base::Value::Type::STRING) {}

LocalSyncPolicyHandler::~LocalSyncPolicyHandler() {}

void LocalSyncPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                 PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::STRING);
  std::string string_value;
  if (value) {
    string_value = value->GetString();
    base::FilePath::StringType expanded_value =
#if BUILDFLAG(IS_WIN)
        policy::path_parser::ExpandPathVariables(
            base::UTF8ToWide(string_value));
#else
        policy::path_parser::ExpandPathVariables(string_value);
#endif
    base::FilePath expanded_path(expanded_value);
    prefs->SetValue(syncer::prefs::kLocalSyncBackendDir,
                    base::Value(expanded_path.AsUTF8Unsafe()));
  }
}

}  // namespace policy
