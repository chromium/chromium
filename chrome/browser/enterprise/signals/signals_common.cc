// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/signals_common.h"

namespace enterprise_signals {

absl::optional<bool> SettingValueToBool(SettingValue setting_value) {
  switch (setting_value) {
    case SettingValue::ENABLED:
      return true;
    case SettingValue::DISABLED:
      return false;
    case SettingValue::UNKNOWN:
      return absl::nullopt;
  }
}

}  // namespace enterprise_signals
