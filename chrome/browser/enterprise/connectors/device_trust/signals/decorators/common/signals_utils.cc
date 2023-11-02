// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_utils.h"

#include <utility>

namespace enterprise_connectors {

base::Value ToListValue(const std::vector<std::string>& string_values) {
  base::Value::List list_value;
  for (const auto& string_value : string_values) {
    list_value.Append(string_value);
  }
  return base::Value(std::move(list_value));
}

}  // namespace enterprise_connectors
