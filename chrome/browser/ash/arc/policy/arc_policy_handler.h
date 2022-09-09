// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_POLICY_ARC_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_POLICY_ARC_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/prefs/pref_value_map.h"

namespace arc {

// |ConfigurationPolicyHandler| implementation for ArcPolicy.
//
// This class issues warnings for unknown managed configuration variables.
class ArcPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  ArcPolicyHandler();

  // ConfigurationPolicyHandler overrides.

  // Adds warning messages to the ArcPolicy entry in |policies| for every
  // unknown managed configuration variable found.
  //
  // TODO(b/223214568) stop abusing PrepareForDisplaying to issue warnings.
  void PrepareForDisplaying(policy::PolicyMap* policies) const override;

 protected:
  // Does nothing. There are no prefs to be applied from ArcPolicy.
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_POLICY_ARC_POLICY_HANDLER_H_
