// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/file_selection_dialogs_policy_handler.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

FileSelectionDialogsPolicyHandler::FileSelectionDialogsPolicyHandler()
    : TypeCheckingPolicyHandler(key::kAllowFileSelectionDialogs,
                                base::Value::Type::BOOLEAN) {}

FileSelectionDialogsPolicyHandler::~FileSelectionDialogsPolicyHandler() {}

void FileSelectionDialogsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (value) {
    bool allow_dialogs = value->GetBool();
    prefs->SetBoolean(prefs::kAllowFileSelectionDialogs, allow_dialogs);
    // Disallow selecting the download location if file dialogs are disabled.
    if (!allow_dialogs)
      prefs->SetBoolean(prefs::kPromptForDownload, false);
  }
}

}  // namespace policy
