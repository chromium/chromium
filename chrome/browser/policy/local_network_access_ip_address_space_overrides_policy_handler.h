// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_LOCAL_NETWORK_ACCESS_IP_ADDRESS_SPACE_OVERRIDES_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_LOCAL_NETWORK_ACCESS_IP_ADDRESS_SPACE_OVERRIDES_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Handles the LocalNetworkAccessIpAddressSpaceOverrides policy.
// Validates that each entry in the list is a valid IP address space override,
// as described by the policy.
class LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler
    : public ListPolicyHandler {
 public:
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler();
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler(
      const LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler&) = delete;
  LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler& operator=(
      const LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler&) = delete;
  ~LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler() override;

 protected:
  // ListPolicyHandler:
  bool CheckListEntry(const base::Value& value) override;
  void ApplyList(base::ListValue filtered_list, PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_LOCAL_NETWORK_ACCESS_IP_ADDRESS_SPACE_OVERRIDES_POLICY_HANDLER_H_
