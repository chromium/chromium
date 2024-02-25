// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/captive_portal_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace network_diagnostics {

namespace mojom = ::chromeos::network_diagnostics::mojom;

class CaptivePortalRoutineTest : public NetworkDiagnosticsTestHelper {
 public:
  CaptivePortalRoutineTest() {
    captive_portal_routine_ = std::make_unique<CaptivePortalRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
  }

  CaptivePortalRoutineTest(const CaptivePortalRoutineTest&) = delete;
  CaptivePortalRoutineTest& operator=(const CaptivePortalRoutineTest&) = delete;

  ~CaptivePortalRoutineTest() override = default;

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::CaptivePortalProblem>& expected_problems,
      base::OnceClosure quit_closure,
      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_captive_portal_problems());
    std::move(quit_closure).Run();
  }

  CaptivePortalRoutine* captive_portal_routine() {
    return captive_portal_routine_.get();
  }

 protected:
  base::WeakPtr<CaptivePortalRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<CaptivePortalRoutine> captive_portal_routine_;
  base::WeakPtrFactory<CaptivePortalRoutineTest> weak_factory_{this};
};

// Test whether an online active network successfully passes.
TEST_F(CaptivePortalRoutineTest, TestNoCaptivePortal) {
  base::RunLoop run_loop;
  SetUpWiFi(shill::kStateOnline);
  std::vector<mojom::CaptivePortalProblem> expected_problems = {};
  captive_portal_routine()->RunRoutine(
      base::BindOnce(&CaptivePortalRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kNoProblem, expected_problems,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

// Test whether no active networks is reported correctly.
TEST_F(CaptivePortalRoutineTest, TestNoActiveNetworks) {
  base::RunLoop run_loop;
  SetUpWiFi(shill::kStateIdle);
  std::vector<mojom::CaptivePortalProblem> expected_problems = {
      mojom::CaptivePortalProblem::kNoActiveNetworks};
  captive_portal_routine()->RunRoutine(
      base::BindOnce(&CaptivePortalRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

// Test that an active network with a suspected portal state is detected.
TEST_F(CaptivePortalRoutineTest, TestPortalSuspected) {
  base::RunLoop run_loop;
  SetUpWiFi(shill::kStatePortalSuspected);
  std::vector<mojom::CaptivePortalProblem> expected_problems = {
      mojom::CaptivePortalProblem::kPortalSuspected};
  captive_portal_routine()->RunRoutine(
      base::BindOnce(&CaptivePortalRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

// Test that an active network behind a portal is detected.
TEST_F(CaptivePortalRoutineTest, TestPortalDetected) {
  base::RunLoop run_loop;
  SetUpWiFi(shill::kStateRedirectFound);
  std::vector<mojom::CaptivePortalProblem> expected_problems = {
      mojom::CaptivePortalProblem::kPortal};
  captive_portal_routine()->RunRoutine(
      base::BindOnce(&CaptivePortalRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

// Test that an active network with no internet is detected.
TEST_F(CaptivePortalRoutineTest, TestNoInternet) {
  base::RunLoop run_loop;
  SetUpWiFi(shill::kStateNoConnectivity);
  std::vector<mojom::CaptivePortalProblem> expected_problems = {
      mojom::CaptivePortalProblem::kNoInternet};
  captive_portal_routine()->RunRoutine(
      base::BindOnce(&CaptivePortalRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

// TODO(khegde): Add a test for unknown captive portal state.

}  // namespace network_diagnostics
}  // namespace ash
