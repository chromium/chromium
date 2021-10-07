// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/net/network_diagnostics/https_latency_routine.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  test::TestEvent<TelemetryData> telemetry_collect_event;
  sampler->CollectTelemetry(telemetry_collect_event.cb());
  routine_ptr->AnalyzeResultsAndExecuteCallback();
  TelemetryData result = telemetry_collect_event.result();

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

  test::TestEvent<TelemetryData> telemetry_collect_event;
  sampler->CollectTelemetry(telemetry_collect_event.cb());
  routine_ptr->AnalyzeResultsAndExecuteCallback();
  TelemetryData result = telemetry_collect_event.result();

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
  test::TestEvent<TelemetryData> telemetry_collect_events[2];
  for (int i = 0; i < 2; ++i) {
    sampler->CollectTelemetry(telemetry_collect_events[i].cb());
  }
  routine_ptr->AnalyzeResultsAndExecuteCallback();

  for (int i = 0; i < 2; ++i) {
    TelemetryData result = telemetry_collect_events[i].result();
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

    test::TestEvent<TelemetryData> telemetry_collect_event;
    sampler->CollectTelemetry(telemetry_collect_event.cb());
    routine_ptr->AnalyzeResultsAndExecuteCallback();
    TelemetryData result = telemetry_collect_event.result();

    EXPECT_EQ(result.networks_telemetry().https_latency_data().verdict(),
              RoutineVerdict::PROBLEM);
    EXPECT_EQ(result.networks_telemetry().https_latency_data().problem(),
              expected_problems[i]);
  }
}
}  // namespace reporting
