// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_structured_metrics_delegate.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class AshStructuredMetricsDelegateTest : public MixinBasedInProcessBrowserTest {
 public:
  AshStructuredMetricsDelegateTest() = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    structured_metrics_mixin_.UpdateRecordingState(true);
  }

 protected:
  StructuredMetricsMixin structured_metrics_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(AshStructuredMetricsDelegateTest,
                       SendValidEventAndSuccessfullyRecords) {
  structured_metrics_mixin_.WaitUntilKeysReady();

  events::v2::test_project_one::TestEventOne test_event;

  EXPECT_THAT(test_event.project_name(), Eq("TestProjectOne"));
  EXPECT_THAT(test_event.event_name(), Eq("TestEventOne"));

  int kTestMetricTwoValue = 1;

  test_event.SetTestMetricOne("hash").SetTestMetricTwo(kTestMetricTwoValue);
  StructuredMetricsClient::Record(std::move(test_event));

  structured_metrics_mixin_.WaitUntilEventRecorded(kProjectOneHash,
                                                   kEventOneHash);

  StructuredEventProto event =
      structured_metrics_mixin_.FindEvent(kProjectOneHash, kEventOneHash)
          .value();

  EXPECT_EQ(event.metrics_size(), 2);
  EXPECT_EQ(event.metrics(0).name_hash(), kMetricOneHash);
  EXPECT_EQ(event.metrics(1).name_hash(), kMetricTwoHash);
  EXPECT_EQ(event.metrics(1).value_int64(), kTestMetricTwoValue);
}

// TODO(jongahn): Add a test that verifies behavior if an invalid event is sent.

}  // namespace metrics::structured
