// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_structured_metrics_recorder.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_events.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics::structured {

namespace {

using EventDelegate = base::RepeatingCallback<void(const Event& event)>;

using testing::Eq;

}  // namespace

class AshStructuredMetricsRecorderTest : public MixinBasedInProcessBrowserTest,
                                         Recorder::RecorderImpl {
 public:
  AshStructuredMetricsRecorderTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    Recorder::GetInstance()->AddObserver(this);
  }

  void TearDownInProcessBrowserTestFixture() override {
    Recorder::GetInstance()->RemoveObserver(this);
    StructuredMetricsClient::Get()->UnsetDelegate();
  }

  void SetTestMessageReceivedClosure(EventDelegate event_delegate) {
    event_delegate_ = event_delegate;
  }

  // RecorderImpl:
  void OnEventRecord(const Event& event) override {
    if (!event_delegate_)
      return;
    event_delegate_.Run(event);
  }

  // Tests do not care about these.
  void OnProfileAdded(const base::FilePath& profile_path) override {}
  void OnReportingStateChanged(bool enabled) override {}
  absl::optional<int> LastKeyRotation(uint64_t project_name_hash) override {
    return absl::nullopt;
  }

 private:
  base::test::ScopedRunLoopTimeout shortened_timeout_{FROM_HERE,
                                                      base::Seconds(3)};

  EventDelegate event_delegate_;
};

IN_PROC_BROWSER_TEST_F(AshStructuredMetricsRecorderTest,
                       SendValidEventAndSuccessfullyRecords) {
  events::v2::test_project_one::TestEventOne test_event;
  test_event.SetTestMetricOne("hash").SetTestMetricTwo(1);

  // Wait for the test messages to have been received.
  base::RunLoop run_loop;
  EventDelegate event_handler =
      base::BindLambdaForTesting([&run_loop](const Event& event) {
        EXPECT_THAT(event.project_name(), Eq("TestProjectOne"));
        EXPECT_THAT(event.event_name(), Eq("TestEventOne"));
        run_loop.Quit();
      });
  SetTestMessageReceivedClosure(event_handler);
  test_event.Record();
  run_loop.Run();
}

// TODO(jongahn): Add a test that verifies behavior if an invalid event is sent.

}  // namespace metrics::structured
