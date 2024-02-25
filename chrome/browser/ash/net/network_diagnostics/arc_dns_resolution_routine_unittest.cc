// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/components/arc/test/fake_net_instance.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/net/network_diagnostics/arc_dns_resolution_routine.h"
#include "content/public/test/browser_task_environment.h"
#include "net/dns/public/dns_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

const int kNoProblemDelayMs = 100;
const int kHighLatencyDelayMs = 450;
const int kVeryHighLatencyDelayMs = 550;

}  // namespace

class ArcDnsResolutionRoutineTest : public ::testing::Test {
 public:
  ArcDnsResolutionRoutineTest() {}

  ArcDnsResolutionRoutineTest(const ArcDnsResolutionRoutineTest&) = delete;
  ArcDnsResolutionRoutineTest& operator=(const ArcDnsResolutionRoutineTest&) =
      delete;
  ~ArcDnsResolutionRoutineTest() override = default;

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::ArcDnsResolutionProblem>& expected_problems,
      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_arc_dns_resolution_problems());
    run_loop_.Quit();
  }

 protected:
  void RunRoutine(
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::ArcDnsResolutionProblem>& expected_problems) {
    arc_dns_resolution_routine_->RunRoutine(
        base::BindOnce(&ArcDnsResolutionRoutineTest::CompareResult, weak_ptr(),
                       expected_routine_verdict, expected_problems));
    run_loop_.Run();
  }

  void SetUpRoutine(arc::mojom::ArcDnsResolutionTestResult result) {
    // Set up the fake NetworkInstance service.
    fake_net_instance_ = std::make_unique<arc::FakeNetInstance>();
    fake_net_instance_->set_dns_resolution_test_result(result);

    // Set up routine with fake NetworkInstance service.
    arc_dns_resolution_routine_ = std::make_unique<ArcDnsResolutionRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
    arc_dns_resolution_routine_->set_net_instance_for_testing(
        fake_net_instance_.get());
  }

  base::WeakPtr<ArcDnsResolutionRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  std::unique_ptr<ArcDnsResolutionRoutine> arc_dns_resolution_routine_;
  std::unique_ptr<arc::FakeNetInstance> fake_net_instance_;
  base::WeakPtrFactory<ArcDnsResolutionRoutineTest> weak_factory_{this};
};

TEST_F(ArcDnsResolutionRoutineTest, TestNoProblem) {
  arc::mojom::ArcDnsResolutionTestResult result;
  result.is_successful = true;
  result.response_code = net::dns_protocol::kRcodeNOERROR;
  result.duration_ms = kNoProblemDelayMs;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(ArcDnsResolutionRoutineTest, TestDnsQueryFailed) {
  arc::mojom::ArcDnsResolutionTestResult result;
  result.is_successful = false;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcDnsResolutionProblem::kFailedDnsQueries});
}

TEST_F(ArcDnsResolutionRoutineTest, TestBadResponseCode) {
  arc::mojom::ArcDnsResolutionTestResult result;
  result.is_successful = true;
  result.response_code = net::dns_protocol::kRcodeREFUSED;
  result.duration_ms = kNoProblemDelayMs;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcDnsResolutionProblem::kFailedDnsQueries});
}

TEST_F(ArcDnsResolutionRoutineTest, TestHighLatency) {
  arc::mojom::ArcDnsResolutionTestResult result;
  result.is_successful = true;
  result.response_code = net::dns_protocol::kRcodeNOERROR;
  result.duration_ms = kHighLatencyDelayMs;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcDnsResolutionProblem::kHighLatency});
}

TEST_F(ArcDnsResolutionRoutineTest, TestVeryHighLatency) {
  arc::mojom::ArcDnsResolutionTestResult result;
  result.is_successful = true;
  result.response_code = net::dns_protocol::kRcodeNOERROR;
  result.duration_ms = kVeryHighLatencyDelayMs;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcDnsResolutionProblem::kVeryHighLatency});
}

}  // namespace network_diagnostics
}  // namespace ash
