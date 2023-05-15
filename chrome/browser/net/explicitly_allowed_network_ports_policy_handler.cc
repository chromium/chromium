// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/explicitly_allowed_network_ports_policy_handler.h"

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "net/base/port_util.h"

namespace policy {

ExplicitlyAllowedNetworkPortsPolicyHandler::
    ExplicitlyAllowedNetworkPortsPolicyHandler()
    : ListPolicyHandler(policy::key::kExplicitlyAllowedNetworkPorts,
                        base::Value::Type::STRING) {}

bool ExplicitlyAllowedNetworkPortsPolicyHandler::CheckListEntry(
    const base::Value& value) {
  const std::string* as_string = value.GetIfString();
  DCHECK(as_string);  // ListPolicyHandler guarantees this.

  int as_int;

  if (!base::StringToInt(*as_string, &as_int)) {
    return false;
  }

  if (!net::IsPortValid(as_int)) {
    return false;
  }

  if (!net::IsAllowablePort(as_int)) {
    return false;
  }

  return true;
}

void ExplicitlyAllowedNetworkPortsPolicyHandler::ApplyList(
    base::Value::List filtered_list,
    PrefValueMap* prefs) {
  base::Value::List integer_list;
  for (const base::Value& value : filtered_list) {
    const std::string& as_string = value.GetString();
    int as_int;
    const bool success = base::StringToInt(as_string, &as_int);
    DCHECK(success);
    integer_list.Append(as_int);
  }
  prefs->SetValue(prefs::kExplicitlyAllowedNetworkPorts,
                  base::Value(std::move(integer_list)));
}

}  // namespace policy
