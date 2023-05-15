// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/metrics_utils.h"

namespace enterprise_connectors {

EnterpriseReportingEventType GetUmaEnumFromEventName(
    const base::StringPiece& eventName) {
  auto* it = kEventNameToUmaEnumMap.find(eventName);
  return it != kEventNameToUmaEnumMap.end()
             ? it->second
             : EnterpriseReportingEventType::kUnknownEvent;
}
}  // namespace enterprise_connectors
