// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/components/arc/test/fake_net_instance.h"
#include "chrome/browser/ash/net/network_diagnostics/arc_ping_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_test_helper.h"
#include "net/dns/public/dns_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

const int kNoProblemDelayMs = 1000;
const int kHighLatencyDelayMs = 1600;

}  // namespace

class ArcPingRoutineTest : public NetworkDiagnosticsTestHelper {
 public:
  ArcPingRoutineTest() = default;
  ArcPingRoutineTest(const ArcPingRoutineTest&) = delete;
  ArcPingRoutineTest& operator=(const ArcPingRoutineTest&) = delete;
  ~ArcPingRoutineTest() override = default;

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::ArcPingProblem>& expected_problems,
      base::OnceClosure quit_closure,
      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems, result->problems->get_arc_ping_problems());
    std::move(quit_closure).Run();
  }

 protected:
  void RunRoutine(mojom::RoutineVerdict expected_routine_verdict,
                  const std::vector<mojom::ArcPingProblem>& expected_problems) {
    base::RunLoop run_loop;
    arc_ping_routine_->RunRoutine(base::BindOnce(
        &ArcPingRoutineTest::CompareResult, weak_ptr(),
        expected_routine_verdict, expected_problems, run_loop.QuitClosure()));
    run_loop.Run();
  }

  void SetUpRoutine(arc::mojom::ArcPingTestResult result) {
    // Set up the fake NetworkInstance service.
    fake_net_instance_ = std::make_unique<arc::FakeNetInstance>();
    fake_net_instance_->set_ping_test_result(result);

    // Set up routine with fake NetworkInstance service.
    arc_ping_routine_ = std::make_unique<ArcPingRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
    arc_ping_routine_->set_net_instance_for_testing(fake_net_instance_.get());
  }

  base::WeakPtr<ArcPingRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<ArcPingRoutine> arc_ping_routine_;
  std::unique_ptr<arc::FakeNetInstance> fake_net_instance_;
  base::WeakPtrFactory<ArcPingRoutineTest> weak_factory_{this};
};

TEST_F(ArcPingRoutineTest, TestNoProblem) {
  arc::mojom::ArcPingTestResult result;
  result.is_successful = true;
  result.duration_ms = kNoProblemDelayMs;

  SetUpRoutine(result);
  SetUpWiFi(shill::kStateOnline);
  RunRoutine(mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(ArcPingRoutineTest, TestUnreachableGateway) {
  arc::mojom::ArcPingTestResult result;
  result.is_successful = false;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcPingProblem::kUnreachableGateway});
}

TEST_F(ArcPingRoutineTest, TestFailedToPingDefaultGateway) {
  arc::mojom::ArcPingTestResult result;
  result.is_successful = false;

  SetUpRoutine(result);
  SetUpWiFi(shill::kStateOnline);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcPingProblem::kFailedToPingDefaultNetwork});
}

TEST_F(ArcPingRoutineTest, TestHighLatencyToPingDefaultGateway) {
  arc::mojom::ArcPingTestResult result;
  result.is_successful = true;
  result.duration_ms = kHighLatencyDelayMs;

  SetUpRoutine(result);
  SetUpWiFi(shill::kStateOnline);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcPingProblem::kDefaultNetworkAboveLatencyThreshold});
}

}  // namespace network_diagnostics
}  // namespace ash
