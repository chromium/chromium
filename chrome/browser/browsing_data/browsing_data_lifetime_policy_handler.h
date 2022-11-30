// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_LIFETIME_POLICY_HANDLER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_LIFETIME_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

// Maps policy to pref like SimpleSchemaValidatingPolicyHandler while ensuring
// that the SyncDisabled policy is set to True.
class BrowsingDataLifetimePolicyHandler
    : public policy::SimpleSchemaValidatingPolicyHandler {
 public:
  BrowsingDataLifetimePolicyHandler(const char* policy_name,
                                    const char* pref_path,
                                    policy::Schema schema);
  ~BrowsingDataLifetimePolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_LIFETIME_POLICY_HANDLER_H_
