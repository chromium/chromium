// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/net/network_diagnostics/https_latency_routine.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

namespace {

using HttpsLatencyRoutine = chromeos::network_diagnostics::HttpsLatencyRoutine;

std::unique_ptr<HttpsLatencyRoutine> HttpsLatencyRoutineGetterTestHelper(
    std::unique_ptr<HttpsLatencyRoutine> routine) {
  return routine;
}
}  // namespace

class FakeHttpsLatencyRoutine
    : public chromeos::network_diagnostics::HttpsLatencyRoutine {
 public:
  FakeHttpsLatencyRoutine() {
    set_verdict(
        chromeos::network_diagnostics::mojom::RoutineVerdict::kNoProblem);
  }

  FakeHttpsLatencyRoutine(
      chromeos::network_diagnostics::mojom::RoutineVerdict verdict,
      chromeos::network_diagnostics::mojom::HttpsLatencyProblem problem) {
    using chromeos::network_diagnostics::mojom::HttpsLatencyProblem;
    using chromeos::network_diagnostics::mojom::RoutineProblems;

    set_verdict(verdict);
    std::vector<HttpsLatencyProblem> problems;
    problems.emplace_back(problem);
    set_problems(RoutineProblems::NewHttpsLatencyProblems(problems));
  }

  ~FakeHttpsLatencyRoutine() override = default;

  void Run() override {}

  void AnalyzeResultsAndExecuteCallback() override { ExecuteCallback(); }
};

TEST(HttpsLatencySamplerTest, NoProblem) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto routine = std::make_unique<FakeHttpsLatencyRoutine>();
  auto* routine_ptr = routine.get();

  auto sampler = std::make_unique<HttpsLatencySampler>();
  sampler->SetHttpsLatencyRoutineGetterForTest(base::BindRepeating(
      &HttpsLatencyRoutineGetterTestHelper, base::Passed(std::move(routine))));

  test::TestEvent<MetricData> metric_collect_event;
  sampler->Collect(metric_collect_event.cb());
  routine_ptr->AnalyzeResultsAndExecuteCallback();
  const auto metric_result = metric_collect_event.result();
  ASSERT_TRUE(metric_result.has_telemetry_data());
  const TelemetryData& result = metric_result.telemetry_data();

  EXPECT_EQ(result.networks_telemetry().https_latency_data().verdict(),
            RoutineVerdict::NO_PROBLEM);
}

TEST(HttpsLatencySamplerTest, FailedRequests) {
  using HttpsLatencyProblemMojom =
      chromeos::network_diagnostics::mojom::HttpsLatencyProblem;
  using RoutineVerdictMojom =
      chromeos::network_diagnostics::mojom::RoutineVerdict;

  base::test::SingleThreadTaskEnvironment task_environment;

  auto routine = std::make_unique<FakeHttpsLatencyRoutine>(
      RoutineVerdictMojom::kProblem,
      HttpsLatencyProblemMojom::kFailedHttpsRequests);
  auto* routine_ptr = routine.get();

  auto sampler = std::make_unique<HttpsLatencySampler>();
  sampler->SetHttpsLatencyRoutineGetterForTest(base::BindRepeating(
      &HttpsLatencyRoutineGetterTestHelper, base::Passed(std::move(routine))));

  test::TestEvent<MetricData> metric_collect_event;
  sampler->Collect(metric_collect_event.cb());
  routine_ptr->AnalyzeResultsAndExecuteCallback();
  const auto metric_result = metric_collect_event.result();
  ASSERT_TRUE(metric_result.has_telemetry_data());
  const TelemetryData& result = metric_result.telemetry_data();

  EXPECT_EQ(result.networks_telemetry().https_latency_data().verdict(),
            RoutineVerdict::PROBLEM);
  EXPECT_EQ(result.networks_telemetry().https_latency_data().problem(),
            HttpsLatencyProblem::FAILED_HTTPS_REQUESTS);
}

TEST(HttpsLatencySamplerTest, OverlappingCalls) {
  using HttpsLatencyProblemMojom =
      chromeos::network_diagnostics::mojom::HttpsLatencyProblem;
  using RoutineVerdictMojom =
      chromeos::network_diagnostics::mojom::RoutineVerdict;

  base::test::SingleThreadTaskEnvironment task_environment;

  auto routine = std::make_unique<FakeHttpsLatencyRoutine>(
      RoutineVerdictMojom::kProblem,
      HttpsLatencyProblemMojom::kFailedDnsResolutions);
  auto* routine_ptr = routine.get();

  auto sampler = std::make_unique<HttpsLatencySampler>();
  sampler->SetHttpsLatencyRoutineGetterForTest(base::BindRepeating(
      &HttpsLatencyRoutineGetterTestHelper, base::Passed(std::move(routine))));
  test::TestEvent<MetricData> metric_collect_events[2];
  for (int i = 0; i < 2; ++i) {
    sampler->Collect(metric_collect_events[i].cb());
  }
  routine_ptr->AnalyzeResultsAndExecuteCallback();

  for (int i = 0; i < 2; ++i) {
    const auto metric_result = metric_collect_events[i].result();
    ASSERT_TRUE(metric_result.has_telemetry_data());
    const TelemetryData& result = metric_result.telemetry_data();

    EXPECT_EQ(result.networks_telemetry().https_latency_data().verdict(),
              RoutineVerdict::PROBLEM);
    EXPECT_EQ(result.networks_telemetry().https_latency_data().problem(),
              HttpsLatencyProblem::FAILED_DNS_RESOLUTIONS);
  }
}

TEST(HttpsLatencySamplerTest, SuccessiveCalls) {
  using HttpsLatencyProblemMojom =
      chromeos::network_diagnostics::mojom::HttpsLatencyProblem;
  using RoutineVerdictMojom =
      chromeos::network_diagnostics::mojom::RoutineVerdict;

  base::test::SingleThreadTaskEnvironment task_environment;

  HttpsLatencyProblemMojom problems[] = {
      HttpsLatencyProblemMojom::kHighLatency,
      HttpsLatencyProblemMojom::kVeryHighLatency};
  HttpsLatencyProblem expected_problems[] = {
      HttpsLatencyProblem::HIGH_LATENCY,
      HttpsLatencyProblem::VERY_HIGH_LATENCY};

  auto sampler = std::make_unique<HttpsLatencySampler>();
  for (int i = 0; i < 2; ++i) {
    auto routine = std::make_unique<FakeHttpsLatencyRoutine>(
        RoutineVerdictMojom::kProblem, problems[i]);
    auto* routine_ptr = routine.get();

    sampler->SetHttpsLatencyRoutineGetterForTest(
        base::BindRepeating(&HttpsLatencyRoutineGetterTestHelper,
                            base::Passed(std::move(routine))));

    test::TestEvent<MetricData> metric_collect_event;
    sampler->Collect(metric_collect_event.cb());
    routine_ptr->AnalyzeResultsAndExecuteCallback();
    const auto metric_result = metric_collect_event.result();
    ASSERT_TRUE(metric_result.has_telemetry_data());
    const TelemetryData& result = metric_result.telemetry_data();

    EXPECT_EQ(result.networks_telemetry().https_latency_data().verdict(),
              RoutineVerdict::PROBLEM);
    EXPECT_EQ(result.networks_telemetry().https_latency_data().problem(),
              expected_problems[i]);
  }
}

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
  previous_latency_data->set_problem(
      HttpsLatencyProblem::FAILED_HTTPS_REQUESTS);
  current_latency_data->set_verdict(RoutineVerdict::PROBLEM);
  current_latency_data->set_problem(HttpsLatencyProblem::FAILED_HTTPS_REQUESTS);

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
}  // namespace reporting
