// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_MOCK_REALTIME_REPORTING_CLIENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_MOCK_REALTIME_REPORTING_CLIENT_H_

#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

class MockRealtimeReportingClient : public RealtimeReportingClient {
 public:
  explicit MockRealtimeReportingClient(content::BrowserContext* context);
  ~MockRealtimeReportingClient() override;
  MockRealtimeReportingClient(const MockRealtimeReportingClient&) = delete;
  MockRealtimeReportingClient& operator=(const MockRealtimeReportingClient&) =
      delete;

  MOCK_METHOD3(ReportRealtimeEvent,
               void(const std::string&,
                    const ReportingSettings& settings,
                    base::Value::Dict event));

  MOCK_METHOD2(ReportEvent,
               void(::chrome::cros::reporting::proto::Event event,
                    const ReportingSettings& settings));

  MOCK_METHOD4(ReportPastEvent,
               void(const std::string& name,
                    const ReportingSettings& settings,
                    base::Value::Dict event,
                    const base::Time& time));

  static std::unique_ptr<KeyedService> CreateMockRealtimeReportingClient(
      content::BrowserContext* context);
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_MOCK_REALTIME_REPORTING_CLIENT_H_
