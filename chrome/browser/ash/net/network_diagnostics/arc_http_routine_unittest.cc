// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/components/arc/test/fake_net_instance.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/net/network_diagnostics/arc_http_routine.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

const int kNoProblemDelayMs = 100;
const int kHighLatencyDelayMs = 550;
const int kVeryHighLatencyDelayMs = 1050;

}  // namespace

class ArcHttpRoutineTest : public ::testing::Test {
 public:
  ArcHttpRoutineTest() {}

  ArcHttpRoutineTest(const ArcHttpRoutineTest&) = delete;
  ArcHttpRoutineTest& operator=(const ArcHttpRoutineTest&) = delete;
  ~ArcHttpRoutineTest() override = default;

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::ArcHttpProblem>& expected_problems,
      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems, result->problems->get_arc_http_problems());
    run_loop_.Quit();
  }

 protected:
  void RunRoutine(mojom::RoutineVerdict expected_routine_verdict,
                  const std::vector<mojom::ArcHttpProblem>& expected_problems) {
    arc_http_routine_->RunRoutine(
        base::BindOnce(&ArcHttpRoutineTest::CompareResult, weak_ptr(),
                       expected_routine_verdict, expected_problems));
    run_loop_.Run();
  }

  void SetUpRoutine(arc::mojom::ArcHttpTestResult result) {
    // Set up the fake NetworkInstance service.
    fake_net_instance_ = std::make_unique<arc::FakeNetInstance>();
    fake_net_instance_->set_http_test_result(result);

    // Set up routine with fake NetworkInstance service.
    arc_http_routine_ = std::make_unique<ArcHttpRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
    arc_http_routine_->set_net_instance_for_testing(fake_net_instance_.get());
  }

  base::WeakPtr<ArcHttpRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  std::unique_ptr<ArcHttpRoutine> arc_http_routine_;
  std::unique_ptr<arc::FakeNetInstance> fake_net_instance_;
  base::WeakPtrFactory<ArcHttpRoutineTest> weak_factory_{this};
};

TEST_F(ArcHttpRoutineTest, TestNoProblem) {
  arc::mojom::ArcHttpTestResult result;
  result.is_successful = true;
  result.status_code = net::HttpStatusCode::HTTP_NO_CONTENT;
  result.duration_ms = kNoProblemDelayMs;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(ArcHttpRoutineTest, TestHttpRequestFailed) {
  arc::mojom::ArcHttpTestResult result;
  result.is_successful = false;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcHttpProblem::kFailedHttpRequests});
}

TEST_F(ArcHttpRoutineTest, TestBadStatusCode) {
  arc::mojom::ArcHttpTestResult result;
  result.is_successful = true;
  result.status_code = net::HttpStatusCode::HTTP_BAD_REQUEST;
  result.duration_ms = kNoProblemDelayMs;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcHttpProblem::kFailedHttpRequests});
}

TEST_F(ArcHttpRoutineTest, TestHighLatency) {
  arc::mojom::ArcHttpTestResult result;
  result.is_successful = true;
  result.status_code = net::HttpStatusCode::HTTP_NO_CONTENT;
  result.duration_ms = kHighLatencyDelayMs;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcHttpProblem::kHighLatency});
}

TEST_F(ArcHttpRoutineTest, TestVeryHighLatency) {
  arc::mojom::ArcHttpTestResult result;
  result.is_successful = true;
  result.status_code = net::HttpStatusCode::HTTP_NO_CONTENT;
  result.duration_ms = kVeryHighLatencyDelayMs;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcHttpProblem::kVeryHighLatency});
}

}  // namespace network_diagnostics
}  // namespace ash
