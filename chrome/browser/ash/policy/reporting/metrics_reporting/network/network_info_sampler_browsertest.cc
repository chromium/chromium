// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/settings/cros_settings_names.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_browsertest_utils.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "content/public/test/browser_test.h"

namespace ash::reporting {

namespace {

using ::chromeos::MissiveClientTestObserver;
using ::reporting::Destination;
using ::reporting::MetricData;
using ::reporting::Priority;
using ::testing::Eq;
using ::testing::NotNull;

class NetworkInfoSamplerBrowserTest : public MetricBrowserTestBase {
 protected:
  NetworkInfoSamplerBrowserTest() = default;
  ~NetworkInfoSamplerBrowserTest() override = default;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(NetworkInfoSamplerBrowserTest,
                       ReportNetworkInfoDefaultDevices) {
  // Default network devices
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      kReportDeviceNetworkInterfaces, true);
  MissiveClientTestObserver observer(Destination::INFO_METRIC);
  // Start initialization after the observer is initialized.
  SetUpDelayedInitialization();

  // Indicates at least one network interface is available.
  bool has_network_interfaces = false;
  do {
    // At least one record, otherwise this line would time out when the loop is
    // entered for the first time.
    auto [priority, record] = observer.GetNextEnqueuedRecord();
    EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
    EXPECT_THAT(record.destination(), Eq(Destination::INFO_METRIC));
    ::reporting::MetricData record_data;
    ASSERT_TRUE(record_data.ParseFromString(record.data()));
    EXPECT_TRUE(record_data.has_timestamp_ms());
    EXPECT_TRUE(record_data.has_info_data());
    EXPECT_FALSE(record_data.has_telemetry_data());

    const auto& info_data = record_data.info_data();
    if (info_data.has_networks_info()) {
      const auto& networks_info = info_data.networks_info();
      has_network_interfaces |= (networks_info.network_interfaces_size() > 0);
    }
  } while (!has_network_interfaces);

  ASSERT_TRUE(has_network_interfaces)
      << "No network interface is in any records.";
}

}  // namespace

}  // namespace ash::reporting
