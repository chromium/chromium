// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_COMMON_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_COMMON_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_signals {

enum class SettingValue {
  UNKNOWN,
  DISABLED,
  ENABLED,
};

// Converts |setting_value| to an optional boolean value. ENABLED and DISABLED
// will be converted to true and false respectively. Other values will be
// treated as missing, and nullopt will be returned instead.
absl::optional<bool> SettingValueToBool(SettingValue setting_value);

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_COMMON_H_
