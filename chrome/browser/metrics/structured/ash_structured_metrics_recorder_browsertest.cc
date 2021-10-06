// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_structured_metrics_recorder.h"

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/metrics/structured/chrome_structured_metrics_recorder.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_mojo_events.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {
namespace structured {

namespace {

using EventDelegate = base::RepeatingCallback<void(const EventBase& event)>;

using testing::Eq;

// Hash of test project to be used.
constexpr uint64_t kProjectOneHash = UINT64_C(16881314472396226433);
constexpr uint64_t kEventOneHash = UINT64_C(13593049295042080097);

}  // namespace

class AshStructuredMetricsRecorderTest : public MixinBasedInProcessBrowserTest,
                                         Recorder::RecorderImpl {
 public:
  AshStructuredMetricsRecorderTest() {
    feature_list_.InitAndEnableFeature(kUseCrosApiInterface);
  }

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
  void OnRecord(const EventBase& event) override {
    // If a delegate has not been assigned, do nothing.
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

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AshStructuredMetricsRecorderTest,
                       SendValidEventAndSuccessfullyRecords) {
  events::v2::test_project_one::TestEventOne test_event;
  test_event.SetTestMetricOne("hash").SetTestMetricTwo(1);

  // Wait for the test messages to have been received.
  base::RunLoop run_loop;
  EventDelegate event_handler =
      base::BindLambdaForTesting([&run_loop](const EventBase& event_base) {
        EXPECT_THAT(event_base.project_name_hash(), Eq(kProjectOneHash));
        EXPECT_THAT(event_base.name_hash(), Eq(kEventOneHash));
        run_loop.Quit();
      });
  SetTestMessageReceivedClosure(event_handler);
  test_event.Record();
  run_loop.Run();
}

// TODO(jongahn): Add a test that verifies behavior if an invalid event is sent.

}  // namespace structured

}  // namespace metrics
