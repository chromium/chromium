// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/mock_realtime_reporting_client.h"
#include "components/enterprise/connectors/core/common.h"

namespace enterprise_connectors::test {

MockRealtimeReportingClient::MockRealtimeReportingClient(
    content::BrowserContext* context)
    : RealtimeReportingClient(context) { }
MockRealtimeReportingClient::~MockRealtimeReportingClient() = default;

// static
std::unique_ptr<KeyedService>
MockRealtimeReportingClient::CreateMockRealtimeReportingClient(
    content::BrowserContext* context) {
  return std::make_unique<MockRealtimeReportingClient>(context);
}

}  // namespace enterprise_connectors::test
