// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/allowed_domains_for_apps_policy_handler.h"

#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

BASE_FEATURE(kAllowedDomainsForAppsNewPolicyHandler,
             "kAllowedDomainsForAppsNewPolicyHandler",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool UseAllowedDomainsForAppsNewPolicyHandler() {
  return base::FeatureList::IsEnabled(kAllowedDomainsForAppsNewPolicyHandler);
}

AllowedDomainsForAppsPolicyHandler::AllowedDomainsForAppsPolicyHandler()
    : policy::ListPolicyHandler(policy::key::kAllowedDomainsForAppsList,
                                base::Value::Type::STRING) {}
AllowedDomainsForAppsPolicyHandler::~AllowedDomainsForAppsPolicyHandler() {}

void AllowedDomainsForAppsPolicyHandler::ApplyList(
    base::Value::List filtered_list,
    PrefValueMap* prefs) {
  std::string list_str;
  for (const auto& item : filtered_list) {
    if (!item.is_string()) {
      NOTREACHED() << "Only string values expected here";
    }
    if (!list_str.empty()) {
      list_str += ",";
    }
    list_str += item.GetString();
  }
  prefs->SetValue(prefs::kAllowedDomainsForApps, base::Value(list_str));
}

// static
void AllowedDomainsForAppsPolicyMigrator::StringToList(base::Value* val) {
  base::Value::List list;
  std::vector<std::string> tokens = base::SplitString(
      val->GetString(), ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& token : tokens) {
    list.Append(token);
  }
  *val = base::Value(std::move(list));
}

void AllowedDomainsForAppsPolicyMigrator::Migrate(PolicyBundle* bundle) {
  policy::PolicyMap& chrome_map =
      bundle->Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, ""));

  const PolicyMigrator::Migration migration = Migration(
      key::kAllowedDomainsForApps, policy::key::kAllowedDomainsForAppsList,
      base::BindRepeating(&StringToList));

  if (!UseAllowedDomainsForAppsNewPolicyHandler()) {
    // If we should revert to only the old policy we need to clear the new one.
    chrome_map.Erase(policy::key::kAllowedDomainsForAppsList);
  }

  CopyPolicyIfUnset(chrome_map, &chrome_map, migration);
}

}  // namespace policy
