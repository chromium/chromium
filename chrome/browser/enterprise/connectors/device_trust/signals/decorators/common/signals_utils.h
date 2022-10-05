// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_SIGNALS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_SIGNALS_UTILS_H_

#include <string>
#include <vector>

#include "base/values.h"

namespace enterprise_connectors {

// Converts `string_values` into a base::Value object containing a list of
// values.
base::Value ToListValue(const std::vector<std::string>& string_values);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_SIGNALS_UTILS_H_
