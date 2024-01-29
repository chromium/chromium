// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/gateway_can_be_pinged_routine.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_test_helper.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

// Fake ICMP output. For more details, see:
// https://gerrit.chromium.org/gerrit/#/c/30310/2/src/helpers/icmp.cc.
const char kFakeValidICMPOutput[] = R"(
    { "4.3.2.1":
      { "sent": 4,
        "recvd": 4,
        "time": 3005,
        "min": 5.789000,
        "avg": 5.913000,
        "max": 6.227000,
        "dev": 0.197000 }
    })";
const char kFakeInvalidICMPOutput[] = R"(
    { "4.3.2.1":
      { "sent": 4,
        "recvd": 4,
        "time": 3005,
        "min": 5.789000,
        "max": 6.227000,
        "dev": 0.197000 }
    })";
const char kFakeLongLatencyICMPOutput[] = R"(
    { "4.3.2.1":
      { "sent": 4,
        "recvd": 4,
        "time": 3005000,
        "min": 5789.000,
        "avg": 5913.000,
        "max": 6227.000,
        "dev": 0.197000 }
    })";

const char kFakeNoReplyICMPOutput[] = R"(
    { "4.3.2.1":
      { "sent": 1,
        "recvd": 0,
        "time": 0,
        "min": 0.000000,
        "avg": 0.000000,
        "max": 0.000000,
        "dev": 0.000000 }
    })";

// This fakes a DebugDaemonClient by serving fake ICMP results when the
// DebugDaemonClient calls TestICMP().
class FakeDebugDaemonClient : public ash::FakeDebugDaemonClient {
 public:
  FakeDebugDaemonClient() = default;

  explicit FakeDebugDaemonClient(const std::string& icmp_output)
      : icmp_output_(icmp_output) {}

  FakeDebugDaemonClient(const FakeDebugDaemonClient&) = delete;
  FakeDebugDaemonClient& operator=(const FakeDebugDaemonClient&) = delete;

  ~FakeDebugDaemonClient() override {}

  void TestICMP(const std::string& ip_address,
                TestICMPCallback callback) override {
    // Invoke the test callback with fake output.
    std::move(callback).Run(std::optional<std::string>{icmp_output_});
  }

 private:
  std::string icmp_output_;
};

}  // namespace

class GatewayCanBePingedRoutineTest : public NetworkDiagnosticsTestHelper {
 public:
  GatewayCanBePingedRoutineTest() = default;
  GatewayCanBePingedRoutineTest(const GatewayCanBePingedRoutineTest&) = delete;
  GatewayCanBePingedRoutineTest& operator=(
      const GatewayCanBePingedRoutineTest&) = delete;
  ~GatewayCanBePingedRoutineTest() override = default;

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::GatewayCanBePingedProblem>& expected_problems,
      base::OnceClosure quit_closure,
      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_gateway_can_be_pinged_problems());
    std::move(quit_closure).Run();
  }

  void SetUpRoutine(const std::string& icmp_output) {
    debug_daemon_client_ = std::make_unique<FakeDebugDaemonClient>(icmp_output);
    gateway_can_be_pinged_routine_ =
        std::make_unique<GatewayCanBePingedRoutine>(
            mojom::RoutineCallSource::kDiagnosticsUI,
            debug_daemon_client_.get());
  }

  GatewayCanBePingedRoutine* gateway_can_be_pinged_routine() {
    return gateway_can_be_pinged_routine_.get();
  }

 protected:
  base::WeakPtr<GatewayCanBePingedRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<GatewayCanBePingedRoutine> gateway_can_be_pinged_routine_;
  std::unique_ptr<FakeDebugDaemonClient> debug_daemon_client_;
  base::WeakPtrFactory<GatewayCanBePingedRoutineTest> weak_factory_{this};
};

TEST_F(GatewayCanBePingedRoutineTest, TestSingleActiveNetwork) {
  SetUpRoutine(kFakeValidICMPOutput);
  SetUpWiFi(shill::kStateOnline);
  std::vector<mojom::GatewayCanBePingedProblem> expected_problems = {};
  base::RunLoop run_loop;
  gateway_can_be_pinged_routine()->RunRoutine(
      base::BindOnce(&GatewayCanBePingedRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kNoProblem, expected_problems,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(GatewayCanBePingedRoutineTest, TestNoActiveNetworks) {
  SetUpRoutine(kFakeValidICMPOutput);
  SetUpWiFi(shill::kStateIdle);
  std::vector<mojom::GatewayCanBePingedProblem> expected_problems = {
      mojom::GatewayCanBePingedProblem::kUnreachableGateway};
  base::RunLoop run_loop;
  gateway_can_be_pinged_routine()->RunRoutine(
      base::BindOnce(&GatewayCanBePingedRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(GatewayCanBePingedRoutineTest, TestFailureToPingDefaultNetwork) {
  // Use |kFakeInvalidICMPOutput| to handle the scenario where a bad ICMP result
  // is received.
  SetUpRoutine(kFakeInvalidICMPOutput);
  SetUpWiFi(shill::kStateOnline);
  std::vector<mojom::GatewayCanBePingedProblem> expected_problems = {
      mojom::GatewayCanBePingedProblem::kFailedToPingDefaultNetwork};
  base::RunLoop run_loop;
  gateway_can_be_pinged_routine()->RunRoutine(
      base::BindOnce(&GatewayCanBePingedRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(GatewayCanBePingedRoutineTest, TestDefaultNetworkAboveLatencyThreshold) {
  // Use |kFakeLongLatencyICMPOutput| to handle the scenario where the ICMP
  // result for the default network is above the threshold.
  SetUpRoutine(kFakeLongLatencyICMPOutput);
  SetUpWiFi(shill::kStateOnline);
  std::vector<mojom::GatewayCanBePingedProblem> expected_problems = {
      mojom::GatewayCanBePingedProblem::kDefaultNetworkAboveLatencyThreshold};
  base::RunLoop run_loop;
  gateway_can_be_pinged_routine()->RunRoutine(
      base::BindOnce(&GatewayCanBePingedRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(GatewayCanBePingedRoutineTest, TestDefaultNetworkNoReply) {
  // Use |kFakeLongLatencyICMPOutput| to handle the scenario where the ICMP
  // result for the default network is above the threshold.
  SetUpRoutine(kFakeNoReplyICMPOutput);
  SetUpWiFi(shill::kStateOnline);
  std::vector<mojom::GatewayCanBePingedProblem> expected_problems = {
      mojom::GatewayCanBePingedProblem::kFailedToPingDefaultNetwork};
  base::RunLoop run_loop;
  gateway_can_be_pinged_routine()->RunRoutine(
      base::BindOnce(&GatewayCanBePingedRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace network_diagnostics
}  // namespace ash
