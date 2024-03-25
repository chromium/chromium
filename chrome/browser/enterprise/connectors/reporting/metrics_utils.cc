// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/metrics_utils.h"

#include <string_view>

namespace enterprise_connectors {

EnterpriseReportingEventType GetUmaEnumFromEventName(
    std::string_view eventName) {
  auto it = kEventNameToUmaEnumMap.find(eventName);
  return it != kEventNameToUmaEnumMap.end()
             ? it->second
             : EnterpriseReportingEventType::kUnknownEvent;
}
}  // namespace enterprise_connectors
