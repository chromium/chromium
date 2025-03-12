// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_router.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screen_ai {

constexpr char kIsSuspendedMetric[] =
    "Accessibility.ScreenAI.Service.IsSuspended";
constexpr char kCrashCountBeforeResumeMetric[] =
    "Accessibility.ScreenAI.Service.CrashCountBeforeResume";

class ScreenAIServiceShutdownHandlerTest : public ::testing::Test {
 public:
  bool IsSuspended() { return router.GetAndRecordSuspendedState(); }
  void DisconnectService() { router.OnScreenAIServiceDisconnected(); }
  void SendShuttingdownMessage() { router.ShuttingDownOnIdle(); }
  bool IsServiceAvailable() {
    std::optional<bool> state =
        router.GetServiceState(ScreenAIServiceRouter::Service::kOCR);
    if (state.has_value()) {
      // A true value means that the service is already running which is not
      // possible in unittest.
      EXPECT_FALSE(state.value());
      return false;
    } else {
      // En empty result means that the service is not banned, and can be used.
      return true;
    }
  }

 protected:
  ScreenAIServiceRouter router;
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ScreenAIServiceShutdownHandlerTest, SuspendedAfterCrash) {
  EXPECT_FALSE(IsSuspended());
  DisconnectService();
  EXPECT_TRUE(IsSuspended());
  EXPECT_FALSE(IsServiceAvailable());
  histogram_tester_.ExpectBucketCount(kIsSuspendedMetric, false, 1);
  // Expect two since it's queried twice in `IsSuspended` and
  // `IsServiceAvailable`.
  histogram_tester_.ExpectBucketCount(kIsSuspendedMetric, true, 2);
}

TEST_F(ScreenAIServiceShutdownHandlerTest, NotSuspendedAfterShutdown) {
  EXPECT_FALSE(IsSuspended());
  SendShuttingdownMessage();
  DisconnectService();
  EXPECT_FALSE(IsSuspended());
  EXPECT_TRUE(IsServiceAvailable());
  // Expect 3 since it's queried twice in `IsSuspended` and once in
  // `IsServiceAvailable`.
  histogram_tester_.ExpectUniqueSample(kIsSuspendedMetric, false, 3);
}

TEST_F(ScreenAIServiceShutdownHandlerTest, CrashCountBeforeResume) {
  EXPECT_FALSE(IsSuspended());
  DisconnectService();
  EXPECT_TRUE(IsSuspended());
  EXPECT_FALSE(IsServiceAvailable());
  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(IsSuspended());
  EXPECT_TRUE(IsServiceAvailable());

  // Revive after crash is only recorded after shutdown message is received and
  // service disconnects.
  SendShuttingdownMessage();
  DisconnectService();
  histogram_tester_.ExpectUniqueSample(kCrashCountBeforeResumeMetric, 1, 1);
}

TEST_F(ScreenAIServiceShutdownHandlerTest, SecondCrashLongerSuspend) {
  EXPECT_FALSE(IsSuspended());
  DisconnectService();
  EXPECT_TRUE(IsSuspended());
  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(IsSuspended());
  // Crash again.
  DisconnectService();
  EXPECT_TRUE(IsSuspended());
  task_environment_.FastForwardBy(base::Minutes(1));
  // Still suspended as the second crash results in longer suspend.
  EXPECT_TRUE(IsSuspended());
  task_environment_.FastForwardBy(base::Minutes(3));
  EXPECT_FALSE(IsSuspended());

  SendShuttingdownMessage();
  DisconnectService();
  histogram_tester_.ExpectUniqueSample(kCrashCountBeforeResumeMetric, 2, 1);
}

}  // namespace screen_ai
