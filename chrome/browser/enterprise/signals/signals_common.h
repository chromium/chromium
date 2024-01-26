// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_COMMON_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_COMMON_H_

#include <optional>

namespace enterprise_signals {

enum class SettingValue {
  UNKNOWN,
  DISABLED,
  ENABLED,
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_COMMON_H_
