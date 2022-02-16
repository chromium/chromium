// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

class FakeNetworkDiagnostics : public NetworkDiagnostics {
 public:
  FakeNetworkDiagnostics() : NetworkDiagnostics(&fake_debug_daemon_client_) {}

  FakeNetworkDiagnostics(const FakeNetworkDiagnostics&) = delete;
  FakeNetworkDiagnostics& operator=(const FakeNetworkDiagnostics&) = delete;

  ~FakeNetworkDiagnostics() override = default;

  void RunHttpsLatency(RunHttpsLatencyCallback callback) override {
    ASSERT_FALSE(callback_);
    callback_ = std::move(callback);
  }

  void ExecuteCallback() {
    // Block until all previously posted tasks are executed to make sure
    // `RunHttpsLatency` is called and `callback_` is set.
    base::RunLoop run_loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_TRUE(callback_);
    std::move(callback_).Run(routine_result_.Clone());
  }

  void SetReceiver(
      mojo::PendingReceiver<NetworkDiagnosticsRoutines> pending_receiver) {
    receiver_ = std::make_unique<mojo::Receiver<NetworkDiagnosticsRoutines>>(
        this, std::move(pending_receiver));
  }

  void SetResultNoProblem(int latency_ms) {
    routine_result_.result_value =
        RoutineResultValue::NewHttpsLatencyResultValue(
            HttpsLatencyResultValue::New(base::Milliseconds(latency_ms)));
    routine_result_.verdict = RoutineVerdictMojom::kNoProblem;
    routine_result_.problems = RoutineProblems::NewHttpsLatencyProblems({});
  }

  void SetResultProblem(HttpsLatencyProblemMojom problem) {
    routine_result_.problems =
        RoutineProblems::NewHttpsLatencyProblems({problem});
    routine_result_.verdict = RoutineVerdictMojom::kProblem;
  }

  void SetResultProblemLatency(HttpsLatencyProblemMojom problem,
                               int latency_ms) {
    routine_result_.result_value =
        RoutineResultValue::NewHttpsLatencyResultValue(
            HttpsLatencyResultValue::New(base::Milliseconds(latency_ms)));
    SetResultProblem(problem);
  }

 private:
  RoutineResult routine_result_;

  std::unique_ptr<mojo::Receiver<NetworkDiagnosticsRoutines>> receiver_;

  RunHttpsLatencyCallback callback_;

  ::chromeos::FakeDebugDaemonClient fake_debug_daemon_client_;
};

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
  FakeNetworkDiagnostics* const fake_diagnostics_;
};

TEST(HttpsLatencySamplerTest, NoProblem) {
  base::test::SingleThreadTaskEnvironment task_environment;

  FakeNetworkDiagnostics diagnostics;
  int latency_ms = 100;
  diagnostics.SetResultNoProblem(latency_ms);
  HttpsLatencySampler sampler(
      std::make_unique<FakeHttpsLatencyDelegate>(&diagnostics));

  test::TestEvent<MetricData> metric_collect_event;
  sampler.Collect(metric_collect_event.cb());
  diagnostics.ExecuteCallback();
  const auto metric_result = metric_collect_event.result();

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
  base::test::SingleThreadTaskEnvironment task_environment;

  FakeNetworkDiagnostics diagnostics;
  diagnostics.SetResultProblem(HttpsLatencyProblemMojom::kFailedHttpsRequests);
  HttpsLatencySampler sampler(
      std::make_unique<FakeHttpsLatencyDelegate>(&diagnostics));

  test::TestEvent<MetricData> metric_collect_event;
  sampler.Collect(metric_collect_event.cb());
  diagnostics.ExecuteCallback();
  const auto metric_result = metric_collect_event.result();

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
  base::test::SingleThreadTaskEnvironment task_environment;

  FakeNetworkDiagnostics diagnostics;
  diagnostics.SetResultProblem(HttpsLatencyProblemMojom::kFailedDnsResolutions);
  HttpsLatencySampler sampler(
      std::make_unique<FakeHttpsLatencyDelegate>(&diagnostics));

  test::TestEvent<MetricData> metric_collect_events[2];
  sampler.Collect(metric_collect_events[0].cb());
  sampler.Collect(metric_collect_events[1].cb());
  diagnostics.ExecuteCallback();

  const auto first_metric_result = metric_collect_events[0].result();
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

  const auto second_metric_result = metric_collect_events[1].result();
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
  base::test::SingleThreadTaskEnvironment task_environment;

  FakeNetworkDiagnostics diagnostics;
  HttpsLatencySampler sampler(
      std::make_unique<FakeHttpsLatencyDelegate>(&diagnostics));

  {
    const int latency_ms = 1000;
    diagnostics.SetResultProblemLatency(HttpsLatencyProblemMojom::kHighLatency,
                                        latency_ms);
    test::TestEvent<MetricData> metric_collect_event;
    sampler.Collect(metric_collect_event.cb());
    diagnostics.ExecuteCallback();
    const auto first_metric_result = metric_collect_event.result();

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
    test::TestEvent<MetricData> metric_collect_event;
    sampler.Collect(metric_collect_event.cb());
    diagnostics.ExecuteCallback();
    const auto second_metric_result = metric_collect_event.result();

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

struct HttpsLatencyEventDetectorTestCase {
  std::string test_name;
  HttpsLatencyProblem problem;
};

class HttpsLatencyEventDetectorTest
    : public ::testing::TestWithParam<HttpsLatencyEventDetectorTestCase> {
 protected:
  void SetNetworkData(std::vector<std::string> connection_states) {
    auto* const service_client = network_handler_test_helper_.service_test();
    auto* const device_client = network_handler_test_helper_.device_test();
    network_handler_test_helper_.profile_test()->AddProfile(kProfilePath,
                                                            kUserHash);
    base::RunLoop().RunUntilIdle();
    network_handler_test_helper_.service_test()->ClearServices();
    network_handler_test_helper_.device_test()->ClearDevices();
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
  base::test::SingleThreadTaskEnvironment task_environment_;

  ::ash::NetworkHandlerTestHelper network_handler_test_helper_;
};

TEST(HttpsLatencyEventDetectorTest, NoEventDetected) {
  MetricData previous_metric_data;
  MetricData current_metric_data;

  HttpsLatencyEventDetector detector;

  auto event_type =
      detector.DetectEvent(previous_metric_data, current_metric_data);

  // No latency data in both current and previous collected data.
  EXPECT_FALSE(event_type.has_value());

  auto* const current_latency_data =
      current_metric_data.mutable_telemetry_data()
          ->mutable_networks_telemetry()
          ->mutable_https_latency_data();
  current_latency_data->set_verdict(RoutineVerdict::NO_PROBLEM);

  event_type = detector.DetectEvent(previous_metric_data, current_metric_data);

  // No latency data in previous collected data and no problems found in current
  // latency data.
  EXPECT_FALSE(event_type.has_value());

  auto* const previous_latency_data =
      previous_metric_data.mutable_telemetry_data()
          ->mutable_networks_telemetry()
          ->mutable_https_latency_data();
  previous_latency_data->set_verdict(RoutineVerdict::NO_PROBLEM);
  current_latency_data->set_verdict(RoutineVerdict::NO_PROBLEM);

  event_type = detector.DetectEvent(previous_metric_data, current_metric_data);

  // No problem found in both previous and current latency data.
  EXPECT_FALSE(event_type.has_value());

  previous_latency_data->set_verdict(RoutineVerdict::PROBLEM);
  previous_latency_data->set_problem(HttpsLatencyProblem::HIGH_LATENCY);
  current_latency_data->set_verdict(RoutineVerdict::PROBLEM);
  current_latency_data->set_problem(HttpsLatencyProblem::HIGH_LATENCY);

  event_type = detector.DetectEvent(previous_metric_data, current_metric_data);

  // Same problem found in both previous and current latency data.
  EXPECT_FALSE(event_type.has_value());
}

TEST(HttpsLatencyEventDetectorTest, EventDetected) {
  MetricEventType expected_event_type =
      MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE;
  MetricData previous_metric_data;
  MetricData current_metric_data;

  HttpsLatencyEventDetector detector;
  auto* const current_latency_data =
      current_metric_data.mutable_telemetry_data()
          ->mutable_networks_telemetry()
          ->mutable_https_latency_data();
  current_latency_data->set_verdict(RoutineVerdict::NO_PROBLEM);

  auto event_type =
      detector.DetectEvent(previous_metric_data, current_metric_data);

  // No event detected, no data previously collected, and current collected data
  // has no problem.
  ASSERT_FALSE(event_type.has_value());

  current_latency_data->set_verdict(RoutineVerdict::PROBLEM);

  event_type = detector.DetectEvent(previous_metric_data, current_metric_data);

  // No data previously collected, and current collected data has problem.
  ASSERT_TRUE(event_type.has_value());
  EXPECT_EQ(event_type.value(), expected_event_type);

  auto* const previous_latency_data =
      previous_metric_data.mutable_telemetry_data()
          ->mutable_networks_telemetry()
          ->mutable_https_latency_data();
  previous_latency_data->set_verdict(RoutineVerdict::PROBLEM);
  current_latency_data->set_verdict(RoutineVerdict::NO_PROBLEM);

  event_type = detector.DetectEvent(previous_metric_data, current_metric_data);

  // Problem found in previous latency data and no problem found in current
  // latency data.
  ASSERT_TRUE(event_type.has_value());
  EXPECT_EQ(event_type.value(), expected_event_type);

  previous_latency_data->set_verdict(RoutineVerdict::NO_PROBLEM);
  current_latency_data->set_verdict(RoutineVerdict::PROBLEM);

  event_type = detector.DetectEvent(previous_metric_data, current_metric_data);

  // No problem found in previous latency data and problem found in current
  // latency data.
  ASSERT_TRUE(event_type.has_value());
  EXPECT_EQ(event_type.value(), expected_event_type);

  previous_latency_data->set_verdict(RoutineVerdict::PROBLEM);
  previous_latency_data->set_problem(HttpsLatencyProblem::HIGH_LATENCY);
  current_latency_data->set_verdict(RoutineVerdict::PROBLEM);
  current_latency_data->set_problem(HttpsLatencyProblem::VERY_HIGH_LATENCY);

  event_type = detector.DetectEvent(previous_metric_data, current_metric_data);

  // Previous and current latency data have different problems.
  ASSERT_TRUE(event_type.has_value());
  EXPECT_EQ(event_type.value(), expected_event_type);
}

TEST_P(HttpsLatencyEventDetectorTest, RequestError_Offline) {
  MetricData previous_metric_data;
  MetricData current_metric_data;

  auto* const current_latency_data =
      current_metric_data.mutable_telemetry_data()
          ->mutable_networks_telemetry()
          ->mutable_https_latency_data();
  current_latency_data->set_verdict(RoutineVerdict::PROBLEM);
  current_latency_data->set_problem(GetParam().problem);

  HttpsLatencyEventDetector detector;

  SetNetworkData({shill::kStateReady, shill::kStateConfiguration});
  auto event_type =
      detector.DetectEvent(previous_metric_data, current_metric_data);

  // No latency data since the request problem is because that the device is not
  // online.
  EXPECT_FALSE(event_type.has_value());
}

TEST_P(HttpsLatencyEventDetectorTest, RequestError_Online) {
  MetricData previous_metric_data;
  MetricData current_metric_data;

  auto* const current_latency_data =
      current_metric_data.mutable_telemetry_data()
          ->mutable_networks_telemetry()
          ->mutable_https_latency_data();
  current_latency_data->set_verdict(RoutineVerdict::PROBLEM);
  current_latency_data->set_problem(GetParam().problem);

  HttpsLatencyEventDetector detector;

  SetNetworkData({shill::kStateReady, shill::kStateOnline});
  auto event_type =
      detector.DetectEvent(previous_metric_data, current_metric_data);

  ASSERT_TRUE(event_type.has_value());
  EXPECT_EQ(event_type.value(), MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE);
}

INSTANTIATE_TEST_SUITE_P(
    HttpsLatencyEventDetectorTests,
    HttpsLatencyEventDetectorTest,
    ::testing::ValuesIn<HttpsLatencyEventDetectorTestCase>(
        {{"FailedDnsResolutions", HttpsLatencyProblem::FAILED_DNS_RESOLUTIONS},
         {"FailedHttpsRequests", HttpsLatencyProblem::FAILED_HTTPS_REQUESTS}}),
    [](const testing::TestParamInfo<HttpsLatencyEventDetectorTest::ParamType>&
           info) { return info.param.test_name; });
}  // namespace
}  // namespace reporting
