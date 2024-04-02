// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_TEST_STRUCTURED_METRICS_MIXIN_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_TEST_STRUCTURED_METRICS_MIXIN_H_

#include <optional>

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/structured_metrics_recorder.h"
#include "components/metrics/structured/test/test_event_storage.h"
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

class StructuredMetricsService;

class StructuredMetricsMixin : public InProcessBrowserTestMixin {
 public:
  explicit StructuredMetricsMixin(InProcessBrowserTestMixinHost* host,
                                  bool setup_profile = true);
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

  // Returns a pointer to the service used.
  structured::StructuredMetricsService* GetService();

  // Returns pointer to the first event with the hash |project_name_hash| and
  // |event_name_hash|. If no event is found, returns std::nullopt.
  std::optional<StructuredEventProto> FindEvent(uint64_t project_name_hash,
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

  // Builds a log of unstaged events.
  std::unique_ptr<ChromeUserMetricsExtension> GetUmaProto();

  EventStorage<StructuredEventProto>* GetEventStorage();

 private:
  std::unique_ptr<MetricsProvider> system_profile_provider_;

  base::ScopedTempDir temp_dir_;

  bool recording_state_ = true;

  // Controls whether a profile is added during setup.
  const bool setup_profile_;

  std::unique_ptr<base::RunLoop> record_run_loop_;
  std::unique_ptr<base::RunLoop> keys_run_loop_;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_TEST_STRUCTURED_METRICS_MIXIN_H_
