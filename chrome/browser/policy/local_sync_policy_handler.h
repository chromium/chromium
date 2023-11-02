// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_LOCAL_SYNC_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_LOCAL_SYNC_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// ConfigurationPolicyHandler for the RoamingProfileLocation policy.
class LocalSyncPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  LocalSyncPolicyHandler();
  LocalSyncPolicyHandler(const LocalSyncPolicyHandler&) = delete;
  LocalSyncPolicyHandler& operator=(const LocalSyncPolicyHandler&) = delete;
  ~LocalSyncPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_LOCAL_SYNC_POLICY_HANDLER_H_
