// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/local_network_access_ip_address_space_overrides_policy_handler.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "services/network/public/cpp/ip_address_space_util.h"

namespace policy {

LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler::
    LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler()
    : ListPolicyHandler(key::kLocalNetworkAccessIpAddressSpaceOverrides,
                        base::Value::Type::STRING) {}

LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler::
    ~LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler() = default;

bool LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler::CheckListEntry(
    const base::Value& value) {
  return network::IsAddressSpaceOverrideValid(value.GetString());
}

void LocalNetworkAccessIpAddressSpaceOverridesPolicyHandler::ApplyList(
    base::ListValue filtered_list,
    PrefValueMap* prefs) {
  prefs->SetValue(prefs::kManagedLocalNetworkAccessIpAddressSpaceOverrides,
                  base::Value(std::move(filtered_list)));
}

}  // namespace policy
