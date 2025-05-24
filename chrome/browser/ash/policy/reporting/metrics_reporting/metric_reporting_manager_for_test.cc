// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager_for_test.h"

#include <memory>

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting::test {

MockDelegate::MockDelegate() = default;

MockDelegate::~MockDelegate() = default;

MetricReportingManagerForTest::MetricReportingManagerForTest(
    std::unique_ptr<Delegate> delegate)
    : MetricReportingManager(std::move(delegate)) {}

MetricReportingManagerForTest::~MetricReportingManagerForTest() = default;

// static
std::unique_ptr<MetricReportingManagerForTest>
MetricReportingManagerForTest::Create(
    std::unique_ptr<Delegate> delegate,
    policy::ManagedSessionService* managed_session_service) {
  auto manager =
      base::WrapUnique(new MetricReportingManagerForTest(std::move(delegate)));
  manager->DelayedInit(managed_session_service);
  return manager;
}

FakeMetricReportQueue* MetricReportingManagerForTest::info_queue() const {
  return static_cast<FakeMetricReportQueue*>(info_report_queue_.get());
}
FakeMetricReportQueue* MetricReportingManagerForTest::telemetry_queue() const {
  return static_cast<FakeMetricReportQueue*>(telemetry_report_queue_.get());
}
FakeMetricReportQueue* MetricReportingManagerForTest::user_telemetry_queue()
    const {
  return static_cast<FakeMetricReportQueue*>(
      user_telemetry_report_queue_.get());
}
FakeMetricReportQueue* MetricReportingManagerForTest::event_queue() const {
  return static_cast<FakeMetricReportQueue*>(event_report_queue_.get());
}
FakeMetricReportQueue* MetricReportingManagerForTest::crash_event_queue()
    const {
  return static_cast<FakeMetricReportQueue*>(crash_event_report_queue_.get());
}
FakeMetricReportQueue* MetricReportingManagerForTest::chrome_crash_event_queue()
    const {
  return static_cast<FakeMetricReportQueue*>(
      chrome_crash_event_report_queue_.get());
}
FakeMetricReportQueue* MetricReportingManagerForTest::user_event_queue() const {
  return static_cast<FakeMetricReportQueue*>(user_event_report_queue_.get());
}
FakeMetricReportQueue* MetricReportingManagerForTest::app_event_queue() const {
  return static_cast<FakeMetricReportQueue*>(app_event_report_queue_.get());
}
FakeMetricReportQueue* MetricReportingManagerForTest::website_event_queue()
    const {
  return static_cast<FakeMetricReportQueue*>(website_event_report_queue_.get());
}
FakeMetricReportQueue*
MetricReportingManagerForTest::user_peripheral_events_and_telemetry_queue()
    const {
  return static_cast<FakeMetricReportQueue*>(
      user_peripheral_events_and_telemetry_report_queue_.get());
}
FakeMetricReportQueue*
MetricReportingManagerForTest::kiosk_heartbeat_telemetry_queue() const {
  return static_cast<FakeMetricReportQueue*>(
      kiosk_heartbeat_telemetry_report_queue_.get());
}
}  // namespace reporting::test
