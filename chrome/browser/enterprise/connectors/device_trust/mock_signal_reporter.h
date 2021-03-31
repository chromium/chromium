// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MOCK_SIGNAL_REPORTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MOCK_SIGNAL_REPORTER_H_

#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"

#include "components/reporting/client/mock_report_queue.h"

namespace enterprise_connectors {

class DeviceTrustSignalReporterForTestBase : public DeviceTrustSignalReporter {
 protected:
  // Invoke this method upon calling PostCreateReportQueueTask to mock queue
  // creation success.
  void CreateMockReportQueueAndCallback(
      reporting::ReportQueueProvider::CreateReportQueueCallback create_queue_cb,
      std::unique_ptr<reporting::ReportQueueConfiguration> config);

  // Invoke this method upon calling PostCreateReportQueueTask to mock queue
  // creation failure.
  void FailCreateReportQueueAndCallback(
      reporting::ReportQueueProvider::CreateReportQueueCallback create_queue_cb,
      std::unique_ptr<reporting::ReportQueueConfiguration> config);

  // Overriding these methods as they rely on CloudPolicyClient in production.
  policy::DMToken GetDmToken() const override;
  // PostCreateReportQueueTask() needs to be override as well but can be
  // customized to mock queue creation success or failure.

  reporting::MockReportQueue* mock_queue_{nullptr};
};

class MockDeviceTrustSignalReporter
    : public DeviceTrustSignalReporterForTestBase {
 public:
  MockDeviceTrustSignalReporter();
  ~MockDeviceTrustSignalReporter() override;

  MOCK_METHOD(void,
              SendReport,
              (base::Value value, base::OnceCallback<void(bool)> sent_cb),
              (override));

 protected:
  void PostCreateReportQueueTask(
      reporting::ReportQueueProvider::CreateReportQueueCallback create_queue_cb,
      std::unique_ptr<reporting::ReportQueueConfiguration> config) override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MOCK_SIGNAL_REPORTER_H_
