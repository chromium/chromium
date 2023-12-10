// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/lacros_structured_metrics_delegate.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/metrics/structured/chrome_structured_metrics_recorder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/structured_events.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::structured {

namespace {

using FlushCallback = base::RepeatingCallback<void()>;
using RecordCallback = base::OnceCallback<void(const Event& event)>;

using testing::Eq;

}  // namespace

// TODO(jongahn): Improve test checkers once callback API is implemented to
// verify that events sent to Ash-Chrome have been verified.
class LacrosStructuredMetricsDelegateTest : public InProcessBrowserTest {
 public:
  LacrosStructuredMetricsDelegateTest() = default;
  ~LacrosStructuredMetricsDelegateTest() override = default;

  class TestObserver : public LacrosStructuredMetricsDelegate::Observer {
   public:
    TestObserver() = default;
    ~TestObserver() override = default;

    void SetFlushCallback(FlushCallback callback) {
      flush_callback_ = std::move(callback);
    }

    void SetRecordCallback(RecordCallback callback) {
      record_callback_ = std::move(callback);
    }

    void OnRecord(const Event& event) override {
      std::move(record_callback_).Run(event);
    }

    void OnFlush() override { std::move(flush_callback_).Run(); }

   private:
    FlushCallback flush_callback_;
    RecordCallback record_callback_;
  };

  void TearDownInProcessBrowserTestFixture() override {
    if (observer_)
      recorder_->RemoveObserver(observer_.get());

    StructuredMetricsClient::Get()->UnsetDelegate();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  TestObserver* InitTestObserver() {
    observer_ = std::make_unique<TestObserver>();
    recorder_ = static_cast<LacrosStructuredMetricsDelegate*>(
        ChromeStructuredMetricsRecorder::Get()->delegate_.get());
    recorder_->AddObserver(observer_.get());
    return observer_.get();
  }

  void RemoveObserver(TestObserver* observer) {
    recorder_->RemoveObserver(observer);
  }

  LacrosStructuredMetricsDelegate* recorder() { return recorder_; }

 private:
  raw_ptr<LacrosStructuredMetricsDelegate> recorder_;
  std::unique_ptr<TestObserver> observer_;
};

IN_PROC_BROWSER_TEST_F(LacrosStructuredMetricsDelegateTest,
                       SendValidEventSuccessfully) {
  auto* observer = InitTestObserver();
  RecordCallback record_callback =
      base::BindLambdaForTesting([](const Event& event) {
        EXPECT_THAT(event.project_name(), "TestProjectOne");
        EXPECT_THAT(event.event_name(), "TestEventOne");
      });
  observer->SetRecordCallback(std::move(record_callback));

  FlushCallback flush_callback = base::BindLambdaForTesting([]() { FAIL(); });
  observer->SetFlushCallback(std::move(flush_callback));

  events::v2::test_project_one::TestEventOne test_event;
  test_event.SetTestMetricOne("hash").SetTestMetricTwo(1);
  test_event.Record();
}

IN_PROC_BROWSER_TEST_F(LacrosStructuredMetricsDelegateTest,
                       EventIgnoredIfSequenceNotSet) {
  auto* observer = InitTestObserver();
  RecordCallback record_callback =
      base::BindLambdaForTesting([](const Event& event) { FAIL(); });
  observer->SetRecordCallback(std::move(record_callback));

  events::v2::test_project_one::TestEventOne test_event;
  test_event.SetTestMetricOne("hash").SetTestMetricTwo(1);

  // Reset the sequence.
  recorder()->SetSequence(nullptr);

  // Record before sequence is set.
  test_event.Record();

  // SUCCESS() if callback not triggered.
}

}  // namespace metrics::structured
