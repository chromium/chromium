// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_structured_metrics_recorder.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/test/test_structured_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics::structured {

namespace {

using EventDelegate = base::RepeatingCallback<void(const Event& event)>;

using testing::Eq;

// The name hash of "TestProjectOne".
constexpr uint64_t kProjectOneHash = UINT64_C(16881314472396226433);

// The name hash of "chrome::TestProjectOne::TestEventOne".
constexpr uint64_t kEventOneHash = UINT64_C(13593049295042080097);

// The name hash of "TestMetricOne".
constexpr uint64_t kMetricOneHash = UINT64_C(637929385654885975);
// The name hash of "TestMetricTwo".
constexpr uint64_t kMetricTwoHash = UINT64_C(14083999144141567134);

}  // namespace

class AshStructuredMetricsRecorderTest : public MixinBasedInProcessBrowserTest {
 public:
  AshStructuredMetricsRecorderTest() = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    structured_metrics_mixin_.GetTestStructuredMetricsProvider()
        ->EnableRecording();
  }

  void TearDownOnMainThread() override {
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();

    // A null callback is needed for when the logout event occurs. If not set,
    // then the test would fail.
    EventDelegate delegate;
    structured_metrics_mixin_.GetTestStructuredMetricsProvider()
        ->SetOnEventsRecordClosure(delegate);
  }

  void AddProfile() {
    structured_metrics_mixin_.GetTestStructuredMetricsProvider()
        ->AddProfilePath(base::FilePath(FILE_PATH_LITERAL("structured_metrics"))
                             .Append(FILE_PATH_LITERAL("user_keys")));
  }

  void WaitUntilInit() {
    // Wait for keys to be initialized and state to be propagated async.
    structured_metrics_mixin_.GetTestStructuredMetricsProvider()
        ->WaitUntilReady();
  }

 protected:
  StructuredMetricsMixin structured_metrics_mixin_{&mixin_host_};

 private:
  base::test::ScopedRunLoopTimeout shortened_timeout_{FROM_HERE,
                                                      base::Seconds(3)};
};

IN_PROC_BROWSER_TEST_F(AshStructuredMetricsRecorderTest,
                       SendValidEventAndSuccessfullyRecords) {
  // Add a profile otherwise event will not be hashed. Then wait for
  // initialization to occur.
  AddProfile();
  WaitUntilInit();

  events::v2::test_project_one::TestEventOne test_event;
  test_event.SetTestMetricOne("hash").SetTestMetricTwo(1);

  // Wait for the test messages to have been received.
  base::RunLoop run_loop;

  base::RepeatingCallback<void(const Event& event)> event_record_callback =
      base::BindLambdaForTesting([&run_loop, this](const Event& event) {
        EXPECT_THAT(event.project_name(), Eq("TestProjectOne"));
        EXPECT_THAT(event.event_name(), Eq("TestEventOne"));
        const StructuredEventProto* event_result =
            structured_metrics_mixin_.GetTestStructuredMetricsProvider()
                ->FindEvent(kProjectOneHash, kEventOneHash)
                .value();
        EXPECT_EQ(event_result->metrics_size(), 2);
        EXPECT_EQ(event_result->metrics(0).name_hash(), kMetricOneHash);
        EXPECT_EQ(event_result->metrics(1).name_hash(), kMetricTwoHash);
        EXPECT_EQ(event_result->metrics(1).value_int64(), 1);
        run_loop.Quit();
      });
  structured_metrics_mixin_.GetTestStructuredMetricsProvider()
      ->SetOnEventsRecordClosure(event_record_callback);
  test_event.Record();
  run_loop.Run();
}

// TODO(jongahn): Add a test that verifies behavior if an invalid event is sent.

}  // namespace metrics::structured
