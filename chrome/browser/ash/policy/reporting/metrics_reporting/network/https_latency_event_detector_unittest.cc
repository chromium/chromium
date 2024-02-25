// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_event_detector.h"

#include <optional>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

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

struct HttpsLatencyEventDetectorTestCase {
  std::string test_name;
  HttpsLatencyProblem problem;
};

class HttpsLatencyEventDetectorTest
    : public ::testing::TestWithParam<HttpsLatencyEventDetectorTestCase> {
 protected:
  base::test::TaskEnvironment task_environment_;
  ::ash::NetworkHandlerTestHelper network_handler_test_helper_;
};

TEST_F(HttpsLatencyEventDetectorTest, NoEventDetected) {
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

  event_type = detector.DetectEvent(std::nullopt, current_metric_data);

  // No previously collected data and no problems found in current latency data.
  EXPECT_FALSE(event_type.has_value());

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

TEST_F(HttpsLatencyEventDetectorTest, EventDetected) {
  MetricEventType expected_event_type =
      MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE;
  MetricData previous_metric_data;
  MetricData current_metric_data;

  HttpsLatencyEventDetector detector;
  auto* const current_latency_data =
      current_metric_data.mutable_telemetry_data()
          ->mutable_networks_telemetry()
          ->mutable_https_latency_data();
  current_latency_data->set_verdict(RoutineVerdict::PROBLEM);

  auto event_type = detector.DetectEvent(std::nullopt, current_metric_data);

  // No data previously collected, and current collected data has problem.
  ASSERT_TRUE(event_type.has_value());
  EXPECT_EQ(event_type.value(), expected_event_type);

  event_type = detector.DetectEvent(previous_metric_data, current_metric_data);

  // No latency data in previous collected data, and current collected data has
  // problem.
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

  SetNetworkData({shill::kStateIdle, shill::kStateConfiguration},
                 &network_handler_test_helper_);
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

  SetNetworkData({shill::kStateOnline, shill::kStateAssociation},
                 &network_handler_test_helper_);
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
