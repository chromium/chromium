// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_EXPLICITLY_ALLOWED_NETWORK_PORTS_POLICY_HANDLER_H_
#define CHROME_BROWSER_NET_EXPLICITLY_ALLOWED_NETWORK_PORTS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Checks and converts the strings in
// policy::key::kExplicitlyAllowedNetworkPorts to integers in
// prefs::kExplicityAllowedNetworkPorts. The reason that the policy uses strings
// is that it permits us to document explicitly what values are supported and
// for how long.
class ExplicitlyAllowedNetworkPortsPolicyHandler final
    : public ListPolicyHandler {
 public:
  ExplicitlyAllowedNetworkPortsPolicyHandler();

 protected:
  // Filters out strings that do not cleanly convert to integers in the port
  // range 1 to 65535.
  bool CheckListEntry(const base::Value& value) override;

  // Converts the values to integers.
  void ApplyList(base::Value::List filtered_list, PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_NET_EXPLICITLY_ALLOWED_NETWORK_PORTS_POLICY_HANDLER_H_
