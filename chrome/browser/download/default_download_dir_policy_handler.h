// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DEFAULT_DOWNLOAD_DIR_POLICY_HANDLER_H_
#define CHROME_BROWSER_DOWNLOAD_DEFAULT_DOWNLOAD_DIR_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

// ConfigurationPolicyHandler for the DefaultDownloadDirectory policy.
class DefaultDownloadDirPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  DefaultDownloadDirPolicyHandler();

  DefaultDownloadDirPolicyHandler(const DefaultDownloadDirPolicyHandler&) =
      delete;
  DefaultDownloadDirPolicyHandler& operator=(
      const DefaultDownloadDirPolicyHandler&) = delete;

  ~DefaultDownloadDirPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;

  void ApplyPolicySettingsWithParameters(
      const policy::PolicyMap& policies,
      const policy::PolicyHandlerParameters& parameters,
      PrefValueMap* prefs) override;

 protected:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DEFAULT_DOWNLOAD_DIR_POLICY_HANDLER_H_
