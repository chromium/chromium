// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_TEST_STRUCTURED_METRICS_MIXIN_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_TEST_STRUCTURED_METRICS_MIXIN_H_

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/structured_metrics_recorder.h"
#include "components/metrics/structured/test/test_structured_metrics_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/metrics_proto/structured_data.pb.h"

// Mixin browser tests can use StructuredMetricsMixin to set up test
// environment for structured metrics recording.
//
// To use the mixin, create it as a member variable in your test, e.g.:
//
//   class MyTest : public MixinBasedInProcessBrowserTest {
//    private:
//     StructuredMetricsMixin structured_metrics_mixin_{&mixin_host_};
namespace metrics::structured {

class StructuredMetricsMixin : public InProcessBrowserTestMixin {
 public:
  explicit StructuredMetricsMixin(InProcessBrowserTestMixinHost* host);
  StructuredMetricsMixin(const StructuredMetricsMixin&) = delete;
  StructuredMetricsMixin& operator=(const StructuredMetricsMixin&) = delete;
  ~StructuredMetricsMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

  // Waits until the event with |project_name_hash| and |event_name_hash| is
  // recorded. Normally, there is a delay between Event::Record() and a flush to
  // disk so the event may not show up immediately, but for tests, all calls to
  // Event::Record() trigger a flush so it should be present immediately.
  void WaitUntilEventRecorded(uint64_t project_name_hash,
                              uint64_t event_name_hash);

  // Waits until the keys have been loaded and the recorder is ready to actively
  // record events.
  void WaitUntilKeysReady();

  // Returns a pointer to the recorder used.
  StructuredMetricsRecorder* GetRecorder();

  // Returns pointer to the first event with the hash |project_name_hash| and
  // |event_name_hash|. If no event is found, returns absl::nullopt.
  absl::optional<StructuredEventProto> FindEvent(uint64_t project_name_hash,
                                                 uint64_t event_name_hash);

  // Returns a vector of pointers to the events with the hash
  // |project_name_hash| and |event_name_hash|.
  std::vector<StructuredEventProto> FindEvents(uint64_t project_name_hash,
                                               uint64_t event_name_hash);

  // Changes the metrics recording state.
  //
  // Note that this will change the recording state for all metrics reporting
  // services.
  void UpdateRecordingState(bool state);

  // Adds a test profile and loads and generates profile keys.
  void AddProfile();

 private:
  // The timeout for this is set to 3 seconds because this should be ample time
  // for the event to show up after Event::Record() has been called. There is
  // normally a delay between flushes but this delay has been set to 0 for
  // testing.
  base::test::ScopedRunLoopTimeout shortened_timeout_{FROM_HERE,
                                                      base::Seconds(3)};

  std::unique_ptr<MetricsProvider> system_profile_provider_;

  std::unique_ptr<TestStructuredMetricsProvider> structured_metrics_provider_;

  base::ScopedTempDir temp_dir_;

  bool recording_state_ = true;

  std::unique_ptr<base::RunLoop> record_run_loop_;
  std::unique_ptr<base::RunLoop> keys_run_loop_;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_TEST_STRUCTURED_METRICS_MIXIN_H_
