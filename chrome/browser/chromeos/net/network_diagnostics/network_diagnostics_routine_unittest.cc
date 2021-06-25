// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_routine.h"

#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace network_diagnostics {

namespace {

constexpr mojom::RoutineVerdict kInitialVerdict =
    mojom::RoutineVerdict::kNotRun;
constexpr mojom::RoutineVerdict kVerdict = mojom::RoutineVerdict::kNoProblem;

}  // namespace

class NetworkDiagnosticsRoutineTest : public ::testing::Test {
 public:
  // Minimal definition for an inherited NetworkDiagnosticsRoutine class.
  class TestNetworkDiagnosticsRoutine : public NetworkDiagnosticsRoutine {
   public:
    TestNetworkDiagnosticsRoutine() = default;
    TestNetworkDiagnosticsRoutine(const TestNetworkDiagnosticsRoutine&) =
        delete;
    TestNetworkDiagnosticsRoutine& operator=(
        const TestNetworkDiagnosticsRoutine&) = delete;
    ~TestNetworkDiagnosticsRoutine() override {}

    // NetworkDiagnosticRoutine:
    void AnalyzeResultsAndExecuteCallback() override {}
  };

  NetworkDiagnosticsRoutineTest() {
    test_network_diagnostics_routine_ =
        std::make_unique<TestNetworkDiagnosticsRoutine>();
  }

  TestNetworkDiagnosticsRoutine* test_network_diagnostics_routine() {
    return test_network_diagnostics_routine_.get();
  }

  mojom::RoutineVerdict verdict() {
    return test_network_diagnostics_routine()->verdict();
  }

  void set_verdict(mojom::RoutineVerdict routine_verdict) {
    test_network_diagnostics_routine()->set_verdict(routine_verdict);
  }

 private:
  std::unique_ptr<TestNetworkDiagnosticsRoutine>
      test_network_diagnostics_routine_;
};

TEST_F(NetworkDiagnosticsRoutineTest, TestVerdictFunctionality) {
  EXPECT_EQ(verdict(), kInitialVerdict);
  set_verdict(kVerdict);
  EXPECT_EQ(verdict(), kVerdict);
}

}  // namespace network_diagnostics
}  // namespace chromeos
