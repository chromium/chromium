// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/fake_network_diagnostics_util.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using ::ash::network_diagnostics::NetworkDiagnostics;
using ::chromeos::network_diagnostics::mojom::HttpsLatencyResultValue;
using ::chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines;
using ::chromeos::network_diagnostics::mojom::RoutineProblems;
using ::chromeos::network_diagnostics::mojom::RoutineResult;
using ::chromeos::network_diagnostics::mojom::RoutineResultValue;

using HttpsLatencyProblemMojom =
    ::chromeos::network_diagnostics::mojom::HttpsLatencyProblem;
using RoutineVerdictMojom =
    ::chromeos::network_diagnostics::mojom::RoutineVerdict;

namespace reporting {
namespace {

// Network service constants.
constexpr char kNetworkName[] = "network_name";
constexpr char kServicePath[] = "/service/path";
constexpr char kDeviceName[] = "device_name";
constexpr char kDevicePath[] = "/device/path";
constexpr char kProfilePath[] = "/profile/path";
constexpr char kGuid[] = "guid";
constexpr char kUserHash[] = "user_hash";

void SetNetworkData(
    std::vector<std::string> connection_states,
    ::ash::NetworkHandlerTestHelper* network_handler_test_helper) {
  auto* const service_client = network_handler_test_helper->service_test();
  auto* const device_client = network_handler_test_helper->device_test();
  network_handler_test_helper->profile_test()->AddProfile(kProfilePath,
                                                          kUserHash);
  base::RunLoop().RunUntilIdle();
  network_handler_test_helper->service_test()->ClearServices();
  network_handler_test_helper->device_test()->ClearDevices();
  for (size_t i = 0; i < connection_states.size(); ++i) {
    std::string index_str = base::StrCat({"_", base::NumberToString(i)});
    const std::string device_path = base::StrCat({kDevicePath, index_str});
    const std::string device_name = base::StrCat({kDeviceName, index_str});
    const std::string service_path = base::StrCat({kServicePath, index_str});
    const std::string network_name = base::StrCat({kNetworkName, index_str});
    const std::string guid = base::StrCat({kGuid, index_str});
    device_client->AddDevice(device_path, shill::kTypeEthernet, device_name);
    base::RunLoop().RunUntilIdle();
    service_client->AddService(service_path, guid, network_name,
                               shill::kTypeEthernet, connection_states[i],
                               /*visible=*/true);
    service_client->SetServiceProperty(service_path, shill::kDeviceProperty,
                                       base::Value(device_path));
    service_client->SetServiceProperty(service_path, shill::kProfileProperty,
                                       base::Value(kProfilePath));
  }
  base::RunLoop().RunUntilIdle();
}

class FakeHttpsLatencyDelegate : public HttpsLatencySampler::Delegate {
 public:
  explicit FakeHttpsLatencyDelegate(FakeNetworkDiagnostics* fake_diagnostics)
      : fake_diagnostics_(fake_diagnostics) {}

  FakeHttpsLatencyDelegate(const FakeHttpsLatencyDelegate&) = delete;
  FakeHttpsLatencyDelegate& operator=(const FakeHttpsLatencyDelegate&) = delete;

  ~FakeHttpsLatencyDelegate() override = default;

  void BindDiagnosticsReceiver(mojo::PendingReceiver<NetworkDiagnosticsRoutines>
                                   pending_receiver) override {
    fake_diagnostics_->SetReceiver(std::move(pending_receiver));
  }

 private:
  const raw_ptr<FakeNetworkDiagnostics> fake_diagnostics_;
};

TEST(HttpsLatencySamplerTest, NoProblem) {
  base::test::TaskEnvironment task_environment;
  ::ash::NetworkHandlerTestHelper network_handler_test_helper;
  SetNetworkData({shill::kStateIdle, shill::kStateOnline},
                 &network_handler_test_helper);

  FakeNetworkDiagnostics diagnostics;
  int latency_ms = 100;
  diagnostics.SetResultNoProblem(latency_ms);
  HttpsLatencySampler sampler(
      std::make_unique<FakeHttpsLatencyDelegate>(&diagnostics));

  test::TestEvent<std::optional<MetricData>> metric_collect_event;
  sampler.MaybeCollect(metric_collect_event.cb());
  diagnostics.ExecuteCallback();
  const std::optional<MetricData> optional_result =
      metric_collect_event.result();

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& metric_result = optional_result.value();

  ASSERT_TRUE(metric_result.has_telemetry_data());
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .verdict(),
            RoutineVerdict::NO_PROBLEM);
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .latency_ms(),
            latency_ms);
  EXPECT_FALSE(metric_result.telemetry_data()
                   .networks_telemetry()
                   .https_latency_data()
                   .has_problem());
}

TEST(HttpsLatencySamplerTest, FailedRequests) {
  base::test::TaskEnvironment task_environment;
  ::ash::NetworkHandlerTestHelper network_handler_test_helper;
  SetNetworkData({shill::kStateOnline, shill::kStateIdle},
                 &network_handler_test_helper);

  FakeNetworkDiagnostics diagnostics;
  diagnostics.SetResultProblem(HttpsLatencyProblemMojom::kFailedHttpsRequests);
  HttpsLatencySampler sampler(
      std::make_unique<FakeHttpsLatencyDelegate>(&diagnostics));

  test::TestEvent<std::optional<MetricData>> metric_collect_event;
  sampler.MaybeCollect(metric_collect_event.cb());
  diagnostics.ExecuteCallback();
  const std::optional<MetricData> optional_result =
      metric_collect_event.result();

  ASSERT_TRUE(optional_result.has_value());
  const MetricData& metric_result = optional_result.value();

  ASSERT_TRUE(metric_result.has_telemetry_data());
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .verdict(),
            RoutineVerdict::PROBLEM);
  EXPECT_FALSE(metric_result.telemetry_data()
                   .networks_telemetry()
                   .https_latency_data()
                   .has_latency_ms());
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .problem(),
            HttpsLatencyProblem::FAILED_HTTPS_REQUESTS);
}

TEST(HttpsLatencySamplerTest, OverlappingCalls) {
  base::test::TaskEnvironment task_environment;
  ::ash::NetworkHandlerTestHelper network_handler_test_helper;
  SetNetworkData({shill::kStateOnline, shill::kStateIdle},
                 &network_handler_test_helper);

  FakeNetworkDiagnostics diagnostics;
  diagnostics.SetResultProblem(HttpsLatencyProblemMojom::kFailedDnsResolutions);
  HttpsLatencySampler sampler(
      std::make_unique<FakeHttpsLatencyDelegate>(&diagnostics));

  test::TestEvent<std::optional<MetricData>> metric_collect_events[2];
  sampler.MaybeCollect(metric_collect_events[0].cb());
  sampler.MaybeCollect(metric_collect_events[1].cb());
  diagnostics.ExecuteCallback();
  const std::optional<MetricData> first_optional_result =
      metric_collect_events[0].result();

  ASSERT_TRUE(first_optional_result.has_value());
  const MetricData& first_metric_result = first_optional_result.value();

  ASSERT_TRUE(first_metric_result.has_telemetry_data());
  EXPECT_EQ(first_metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .verdict(),
            RoutineVerdict::PROBLEM);
  EXPECT_FALSE(first_metric_result.telemetry_data()
                   .networks_telemetry()
                   .https_latency_data()
                   .has_latency_ms());
  EXPECT_EQ(first_metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .problem(),
            HttpsLatencyProblem::FAILED_DNS_RESOLUTIONS);

  const std::optional<MetricData> second_optional_result =
      metric_collect_events[1].result();

  ASSERT_TRUE(second_optional_result.has_value());
  const MetricData& second_metric_result = second_optional_result.value();

  ASSERT_TRUE(second_metric_result.has_telemetry_data());
  EXPECT_EQ(second_metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .verdict(),
            RoutineVerdict::PROBLEM);
  EXPECT_FALSE(second_metric_result.telemetry_data()
                   .networks_telemetry()
                   .https_latency_data()
                   .has_latency_ms());
  EXPECT_EQ(second_metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .problem(),
            HttpsLatencyProblem::FAILED_DNS_RESOLUTIONS);
}

TEST(HttpsLatencySamplerTest, SuccessiveCalls) {
  base::test::TaskEnvironment task_environment;
  ::ash::NetworkHandlerTestHelper network_handler_test_helper;
  SetNetworkData({shill::kStateIdle, shill::kStateOnline},
                 &network_handler_test_helper);

  FakeNetworkDiagnostics diagnostics;
  HttpsLatencySampler sampler(
      std::make_unique<FakeHttpsLatencyDelegate>(&diagnostics));

  {
    const int latency_ms = 1000;
    diagnostics.SetResultProblemLatency(HttpsLatencyProblemMojom::kHighLatency,
                                        latency_ms);
    test::TestEvent<std::optional<MetricData>> metric_collect_event;
    sampler.MaybeCollect(metric_collect_event.cb());
    diagnostics.ExecuteCallback();
    const std::optional<MetricData> first_optional_result =
        metric_collect_event.result();

    ASSERT_TRUE(first_optional_result.has_value());
    const MetricData& first_metric_result = first_optional_result.value();

    ASSERT_TRUE(first_metric_result.has_telemetry_data());
    EXPECT_EQ(first_metric_result.telemetry_data()
                  .networks_telemetry()
                  .https_latency_data()
                  .verdict(),
              RoutineVerdict::PROBLEM);
    EXPECT_EQ(first_metric_result.telemetry_data()
                  .networks_telemetry()
                  .https_latency_data()
                  .latency_ms(),
              latency_ms);
    EXPECT_EQ(first_metric_result.telemetry_data()
                  .networks_telemetry()
                  .https_latency_data()
                  .problem(),
              HttpsLatencyProblem::HIGH_LATENCY);
  }

  {
    const int latency_ms = 5000;
    diagnostics.SetResultProblemLatency(
        HttpsLatencyProblemMojom::kVeryHighLatency, latency_ms);
    test::TestEvent<std::optional<MetricData>> metric_collect_event;
    sampler.MaybeCollect(metric_collect_event.cb());
    diagnostics.ExecuteCallback();
    const std::optional<MetricData> second_optional_result =
        metric_collect_event.result();

    ASSERT_TRUE(second_optional_result.has_value());
    const MetricData& second_metric_result = second_optional_result.value();

    ASSERT_TRUE(second_metric_result.has_telemetry_data());
    EXPECT_EQ(second_metric_result.telemetry_data()
                  .networks_telemetry()
                  .https_latency_data()
                  .verdict(),
              RoutineVerdict::PROBLEM);
    EXPECT_EQ(second_metric_result.telemetry_data()
                  .networks_telemetry()
                  .https_latency_data()
                  .latency_ms(),
              latency_ms);
    EXPECT_EQ(second_metric_result.telemetry_data()
                  .networks_telemetry()
                  .https_latency_data()
                  .problem(),
              HttpsLatencyProblem::VERY_HIGH_LATENCY);
  }
}

TEST(HttpsLatencySamplerTest, Offline) {
  base::test::TaskEnvironment task_environment;
  ::ash::NetworkHandlerTestHelper network_handler_test_helper;
  SetNetworkData({shill::kStateReady, shill::kStateConfiguration},
                 &network_handler_test_helper);

  FakeNetworkDiagnostics diagnostics;
  diagnostics.SetResultProblem(HttpsLatencyProblemMojom::kFailedHttpsRequests);
  HttpsLatencySampler sampler(
      std::make_unique<FakeHttpsLatencyDelegate>(&diagnostics));
  bool callback_called = false;
  std::optional<MetricData> metric_data_result;

  sampler.MaybeCollect(
      base::BindLambdaForTesting([&callback_called, &metric_data_result](
                                     std::optional<MetricData> metric_data) {
        callback_called = true;
        metric_data_result = std::move(metric_data);
      }));

  ASSERT_TRUE(callback_called);
  EXPECT_FALSE(metric_data_result.has_value());
}
}  // namespace
}  // namespace reporting
