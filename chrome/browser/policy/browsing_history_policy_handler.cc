// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browsing_history_policy_handler.h"

#include "base/values.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

BrowsingHistoryPolicyHandler::BrowsingHistoryPolicyHandler()
    : TypeCheckingPolicyHandler(key::kAllowDeletingBrowserHistory,
                                base::Value::Type::BOOLEAN) {}

BrowsingHistoryPolicyHandler::~BrowsingHistoryPolicyHandler() {}

void BrowsingHistoryPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (value && !value->GetBool()) {
    prefs->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, false);
    prefs->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistoryBasic, false);
    prefs->SetBoolean(browsing_data::prefs::kDeleteDownloadHistory, false);
  }
}

}  // namespace policy
