// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_FILE_SELECTION_DIALOGS_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_FILE_SELECTION_DIALOGS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// ConfigurationPolicyHandler for the FileSelectionDialogs policy.
class FileSelectionDialogsPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  FileSelectionDialogsPolicyHandler();
  FileSelectionDialogsPolicyHandler(const FileSelectionDialogsPolicyHandler&) =
      delete;
  FileSelectionDialogsPolicyHandler& operator=(
      const FileSelectionDialogsPolicyHandler&) = delete;
  ~FileSelectionDialogsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_FILE_SELECTION_DIALOGS_POLICY_HANDLER_H_
