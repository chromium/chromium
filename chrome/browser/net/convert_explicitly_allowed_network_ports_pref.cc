// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "convert_explicitly_allowed_network_ports_pref.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

std::vector<uint16_t> ConvertExplicitlyAllowedNetworkPortsPref(
    PrefService* local_state) {
  std::vector<uint16_t> explicitly_allowed_network_ports;
  const base::Value::List& explicitly_allowed_network_ports_list =
      local_state->GetList(prefs::kExplicitlyAllowedNetworkPorts);
  if (explicitly_allowed_network_ports_list.empty()) {
    return explicitly_allowed_network_ports;
  }
  explicitly_allowed_network_ports.reserve(
      explicitly_allowed_network_ports_list.size());
  for (const base::Value& value : explicitly_allowed_network_ports_list) {
    const std::optional<int> optional_int = value.GetIfInt();
    if (!optional_int) {
      // We handle this case because prefs can be corrupt, but it shouldn't
      // happen normally.
      DLOG(WARNING) << "Ignoring non-int value";
      continue;
    }

    const int int_value = optional_int.value();
    if (int_value < 1 || int_value > 65535) {
      // Out of range for a port number. Ignored.
      DLOG(WARNING) << "Ignoring out-of-range value: " << int_value;
      continue;
    }

    explicitly_allowed_network_ports.push_back(
        static_cast<uint16_t>(int_value));
  }
  return explicitly_allowed_network_ports;
}
