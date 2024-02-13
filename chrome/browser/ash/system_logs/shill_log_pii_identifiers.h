// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_SHILL_LOG_PII_IDENTIFIERS_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_SHILL_LOG_PII_IDENTIFIERS_H_

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "components/feedback/redaction_tool/pii_types.h"

namespace system_logs {

// Maps PII in properties (match src/platform2/modem-utilities/connectivity)
// to a specific PIIType. For Shill logs we rely on intelligent anonymous
// replacements for IP and MAC addresses and BSSID in
// components/feedback/anonymizer_tool.cc.
extern const base::
    fixed_flat_map<std::string_view, redaction::PIIType, 19, std::less<>>
        kShillPIIMaskedMap;

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_SHILL_LOG_PII_IDENTIFIERS_H_
