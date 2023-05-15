// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_LIFETIME_POLICY_HANDLER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_LIFETIME_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/sync/base/user_selectable_type.h"

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
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
  void PrepareForDisplaying(policy::PolicyMap* policies) const override;

 private:
  // Caches sync types required when the policy is checked, to
  // avoid recomputing when it is applied or prepared for display.
  syncer::UserSelectableTypeSet forced_disabled_sync_types_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_LIFETIME_POLICY_HANDLER_H_
