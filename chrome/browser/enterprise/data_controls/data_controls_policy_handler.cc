// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/data_controls_policy_handler.h"

#include "components/prefs/pref_value_map.h"

namespace data_controls {

DataControlsPolicyHandler::DataControlsPolicyHandler(const char* policy_name,
                                                     const char* pref_path,
                                                     policy::Schema schema)
    : policy::CloudOnlyPolicyHandler(
          policy_name,
          schema.GetKnownProperty(policy_name),
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN),
      pref_path_(pref_path) {}
DataControlsPolicyHandler::~DataControlsPolicyHandler() = default;

void DataControlsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_) {
    return;
  }
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (value) {
    prefs->SetValue(pref_path_, value->Clone());
  }
}

}  // namespace data_controls
