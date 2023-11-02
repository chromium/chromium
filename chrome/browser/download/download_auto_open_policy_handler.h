// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_AUTO_OPEN_POLICY_HANDLER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_AUTO_OPEN_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/schema.h"

class PrefValueMap;

namespace policy {
class PolicyMap;
}  // namespace policy

class DownloadAutoOpenPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit DownloadAutoOpenPolicyHandler(const policy::Schema& chrome_schema);
  ~DownloadAutoOpenPolicyHandler() override;

  DownloadAutoOpenPolicyHandler(const DownloadAutoOpenPolicyHandler&) = delete;
  DownloadAutoOpenPolicyHandler& operator=(
      const DownloadAutoOpenPolicyHandler&) = delete;

  // SchemaValidatingPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_AUTO_OPEN_POLICY_HANDLER_H_
