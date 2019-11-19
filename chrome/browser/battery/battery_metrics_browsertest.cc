// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/battery_status.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/service_binding.h"

namespace {

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramBucketUntilCountReached(
    base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    base::HistogramBase::Sample target_bucket,
    size_t count) {
  base::RunLoop().RunUntilIdle();
  for (size_t attempt = 0; attempt < 50; ++attempt) {
    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      if (bucket.min == target_bucket)
        total_count += bucket.count;
    }
    if (total_count >= count)
      return;
    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

// Replaces the platform specific implementation of BatteryMonitor.
class MockBatteryMonitor : public device::mojom::BatteryMonitor {
 public:
  MockBatteryMonitor() = default;
  ~MockBatteryMonitor() override = default;

  void Bind(mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(std::move(receiver));
  }

  void DidChange(const device::mojom::BatteryStatus& battery_status) {
    status_ = battery_status;
    status_to_report_ = true;

    if (!callback_.is_null())
      ReportStatus();
  }

  void CloseReceiver() { receiver_.reset(); }

 private:
  // mojom::BatteryMonitor methods:
  void QueryNextStatus(QueryNextStatusCallback callback) override {
    if (!callback_.is_null()) {
      receiver_.reset();
      return;
    }
    callback_ = std::move(callback);

    if (status_to_report_)
      ReportStatus();
  }

  void ReportStatus() {
    std::move(callback_).Run(status_.Clone());
    status_to_report_ = false;
  }

  QueryNextStatusCallback callback_;
  device::mojom::BatteryStatus status_;
  bool status_to_report_ = false;
  mojo::Receiver<device::mojom::BatteryMonitor> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(MockBatteryMonitor);
};

// Test that the metricss around battery usage are recorded correctly.
class BatteryMetricsBrowserTest : public InProcessBrowserTest {
 public:
  BatteryMetricsBrowserTest() = default;
  ~BatteryMetricsBrowserTest() override = default;

 protected:
  MockBatteryMonitor* mock_battery_monitor() {
    return mock_battery_monitor_.get();
  }

 private:
  void SetUpOnMainThread() override {
    mock_battery_monitor_ = std::make_unique<MockBatteryMonitor>();
    service_manager::ServiceBinding::OverrideInterfaceBinderForTesting(
        device::mojom::kServiceName,
        base::BindRepeating(&MockBatteryMonitor::Bind,
                            base::Unretained(mock_battery_monitor_.get())));
  }

  void TearDownOnMainThread() override {
    service_manager::ServiceBinding::ClearInterfaceBinderOverrideForTesting<
        device::mojom::BatteryMonitor>(device::mojom::kServiceName);

    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<MockBatteryMonitor> mock_battery_monitor_;

  DISALLOW_COPY_AND_ASSIGN(BatteryMetricsBrowserTest);
};

#if defined(OS_WIN)
#define DISABLED_ON_WIN(name) DISABLED##name
#else
#define DISABLED_ON_WIN(name) name
#endif

IN_PROC_BROWSER_TEST_F(BatteryMetricsBrowserTest,
                       DISABLED_ON_WIN(BatteryDropUMA)) {
  // Verify that drops in battery level are recorded, and drops by less than 1%
  // are aggregated together until there is a full percentage drop.
  device::mojom::BatteryStatus status;
  status.charging = false;
  status.charging_time = std::numeric_limits<double>::infinity();
  status.discharging_time = 100;
  status.level = 0.6;

  mock_battery_monitor()->DidChange(status);

  base::HistogramTester histogram_tester;
  // A drop of 10.9% should record in the 10 bucket.
  status.level = 0.491;
  mock_battery_monitor()->DidChange(status);
  RetryForHistogramBucketUntilCountReached(&histogram_tester,
                                           "Power.BatteryPercentDrop", 10, 1);

  // .9% should be stored from the previous drop, so an additional drop to 1%
  // should be recorded in the 1% bucket.
  status.level = 0.49;
  mock_battery_monitor()->DidChange(status);

  RetryForHistogramBucketUntilCountReached(&histogram_tester,
                                           "Power.BatteryPercentDrop", 1, 1);
}

}  // namespace
