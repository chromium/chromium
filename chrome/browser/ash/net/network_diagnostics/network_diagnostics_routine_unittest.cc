// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

constexpr mojom::RoutineVerdict kInitialVerdict =
    mojom::RoutineVerdict::kNotRun;
constexpr mojom::RoutineVerdict kVerdict = mojom::RoutineVerdict::kNoProblem;

constexpr mojom::RoutineType kType = mojom::RoutineType::kHttpFirewall;

constexpr mojom::RoutineCallSource kSource =
    mojom::RoutineCallSource::kDiagnosticsUI;

}  // namespace

class NetworkDiagnosticsRoutineTest : public ::testing::Test {
 public:
  // Minimal definition for an inherited NetworkDiagnosticsRoutine class.
  class TestNetworkDiagnosticsRoutine : public NetworkDiagnosticsRoutine {
   public:
    explicit TestNetworkDiagnosticsRoutine(
        chromeos::network_diagnostics::mojom::RoutineCallSource source)
        : NetworkDiagnosticsRoutine(source) {}
    TestNetworkDiagnosticsRoutine(const TestNetworkDiagnosticsRoutine&) =
        delete;
    TestNetworkDiagnosticsRoutine& operator=(
        const TestNetworkDiagnosticsRoutine&) = delete;
    ~TestNetworkDiagnosticsRoutine() override {}

    // NetworkDiagnosticRoutine:
    mojom::RoutineType Type() override { return kType; }
    void Run() override { ExecuteCallback(); }
    void AnalyzeResultsAndExecuteCallback() override {}
  };

  NetworkDiagnosticsRoutineTest() {
    test_network_diagnostics_routine_ =
        std::make_unique<TestNetworkDiagnosticsRoutine>(
            mojom::RoutineCallSource::kUnknown);
  }

  TestNetworkDiagnosticsRoutine* test_network_diagnostics_routine() {
    return test_network_diagnostics_routine_.get();
  }

  void set_verdict(mojom::RoutineVerdict verdict) {
    test_network_diagnostics_routine()->set_verdict(verdict);
  }

  void set_problems(mojom::RoutineProblemsPtr problems) {
    test_network_diagnostics_routine()->set_problems(problems.Clone());
  }

  void set_can_run(bool can_run) { can_run_ = can_run; }

  void set_source(mojom::RoutineCallSource source) {
    test_network_diagnostics_routine()->set_source_for_testing(source);
  }

 private:
  std::unique_ptr<TestNetworkDiagnosticsRoutine>
      test_network_diagnostics_routine_;
  bool can_run_ = true;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(NetworkDiagnosticsRoutineTest, TestDefaultRoutine) {
  TestNetworkDiagnosticsRoutine* routine = test_network_diagnostics_routine();
  set_verdict(kVerdict);
  base::RunLoop run_loop;

  routine->RunRoutine(
      base::BindLambdaForTesting([&](mojom::RoutineResultPtr result) {
        EXPECT_EQ(result->verdict, kVerdict);
        EXPECT_FALSE(result->timestamp.is_null());
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(NetworkDiagnosticsRoutineTest, NotRunRoutine) {
  TestNetworkDiagnosticsRoutine* routine = test_network_diagnostics_routine();
  set_can_run(false);
  base::RunLoop run_loop;

  routine->RunRoutine(
      base::BindLambdaForTesting([&](mojom::RoutineResultPtr result) {
        EXPECT_EQ(result->verdict, kInitialVerdict);
        EXPECT_FALSE(result->timestamp.is_null());
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(NetworkDiagnosticsRoutineTest, CorrectSource) {
  TestNetworkDiagnosticsRoutine* routine = test_network_diagnostics_routine();
  set_source(kSource);
  base::RunLoop run_loop;

  routine->RunRoutine(
      base::BindLambdaForTesting([&](mojom::RoutineResultPtr result) {
        EXPECT_EQ(result->source, kSource);
        EXPECT_EQ(result->verdict, kInitialVerdict);
        EXPECT_FALSE(result->timestamp.is_null());
        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace network_diagnostics
}  // namespace ash
