// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_RESPONSIVENESS_CALCULATOR_H_
#define BASE_TASK_RESPONSIVENESS_CALCULATOR_H_

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/base_export.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/responsiveness/responsiveness_calculator_delegate.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace base {
namespace responsiveness {

class SecondarySources;

// Aggregates `(queue_time, execution_start_time, execution_finish_time)`
// tuples from `Watcher` (on the main thread and optionally via
// `SecondarySources`) into responsiveness histograms and trace events,
// accounting for startup stages (`OnFirstIdle()`) and process suspension.
//
// Splits each 30s period into 300 100ms slices. For a congested task/event,
// each point `t = start + k * 100ms` (k >= 1, `t < execution_finish_time`,
// where `start` is `queue_time`, or `execution_start_time` for `.RunningOnly`)
// marks its containing 100ms slice as congested:
// - Union (`<histogram_prefix>.MainThreadsCongestion*`): slices congested on
//   at least one monitored source.
// - Breakdown (`<histogram_prefix>.MainThreadsCongestion.<source_name>*`):
//   slices congested on a specific named source.
//
// All members are main-thread affine.
class BASE_EXPORT Calculator {
 public:
  struct Config {
    // Optional breakdown histogram suffix for the main thread. When non-empty,
    // main-thread congestion is also emitted to
    // `<histogram_prefix>.MainThreadsCongestion.<main_source_name>*` (in
    // addition to the union histograms).
    std::string_view main_source_name = "";
    // Whether to also aggregate congestions from `SecondarySources::Get()`
    // (which must be non-null and outlive `this` when true; the main thread is
    // tracked directly by `Calculator` and must not be registered there). When
    // false, emitted histograms and trace events reflect only main-thread
    // congestion.
    bool consume_secondary_sources = false;
    // Whether to observe `ApplicationStatusListener` on Android to treat
    // backgrounded browser application states as suspended.
    bool observe_application_status = false;
  };

  Calculator(std::unique_ptr<ResponsivenessCalculatorDelegate> delegate,
             std::string_view histogram_prefix);
  Calculator(std::unique_ptr<ResponsivenessCalculatorDelegate> delegate,
             std::string_view histogram_prefix,
             Config config);

  Calculator(const Calculator&) = delete;
  Calculator& operator=(const Calculator&) = delete;

  virtual ~Calculator();

  // Must be called from the main thread. Virtual for testing.
  // Assumes that |execution_finish_time| is the current time.
  // The implementation will gracefully handle successive calls with unordered
  // |queue_time|s.
  virtual void TaskOrEventFinishedOnMainThread(
      base::TimeTicks queue_time,
      base::TimeTicks execution_start_time,
      base::TimeTicks execution_finish_time);

  // Must be invoked once-and-only-once on the main thread, after it first
  // reaches idle (done running tasks queued during startup). Signals the end of
  // startup and starts recording queuing congestion (for all sources). Virtual
  // for testing.
  virtual void OnFirstIdle();

  // Each congested task/event is fully defined by |start_time| and |end_time|.
  // Note that |duration| = |end_time| - |start_time|.
  struct Congestion {
    Congestion(base::TimeTicks start_time, base::TimeTicks end_time);

    base::TimeTicks start_time;
    base::TimeTicks end_time;
  };

  using CongestionList = std::vector<Congestion>;

  // Congestions accumulated for a single source, with optional breakdown
  // histogram suffix `name` (empty = union histograms only). Not thread-safe
  // (guarded by main-thread affinity in `Calculator` and by `lock_` in
  // `SecondarySources`).
  struct BASE_EXPORT SourceCongestions {
    explicit SourceCongestions(std::string_view name);
    SourceCongestions(std::string name,
                      CongestionList execution_congestion,
                      CongestionList congestion);
    SourceCongestions(SourceCongestions&&);
    SourceCongestions& operator=(SourceCongestions&&);
    ~SourceCongestions();

    SourceCongestions TakeOlderThanTime(base::TimeTicks end_time);
    void Clear();

    std::string name;
    CongestionList execution_congestion;
    CongestionList congestion;
  };

  // Types of congestion recorded by this Calculator. Public for testing.
  enum class CongestionType {
    kExecutionOnly,
    kQueueAndExecution,
  };

  // Stages of startup used by this Calculator. Stages are defined in
  // chronological order, some can be skipped. Public for
  // testing.
  enum class StartupStage {
    // Monitoring the first interval.
    kFirstInterval,
    // First interval completed but it didn't capture OnFirstIdle().
    kFirstIntervalDoneWithoutFirstIdle,
    // Monitoring the first interval after OnFirstIdle().
    kFirstIntervalAfterFirstIdle,
    // All intervals after kFirstIntervalAfterFirstIdle.
    kPeriodic
  };

 protected:
  // Emits an UMA metric for responsiveness of a single measurement interval.
  // Exposed for testing.
  virtual void EmitResponsiveness(CongestionType congestion_type,
                                  size_t num_congested_slices,
                                  uint64_t event_id);

  // Emits trace events for responsiveness metric. A trace event is emitted for
  // the whole duration of the metric interval and sub events are emitted for
  // the specific congested slices.
  // Exposed for testing.
  void EmitResponsivenessTraceEvents(CongestionType congestion_type,
                                     base::TimeTicks start_time,
                                     base::TimeTicks end_time,
                                     const std::set<int>& congested_slices,
                                     uint64_t event_id);

  // Exposed for testing.
  virtual void EmitCongestedIntervalsMeasurementTraceEvent(
      base::TimeTicks start_time,
      base::TimeTicks end_time,
      size_t num_congested_slices,
      uint64_t event_id);

  // Exposed for testing.
  virtual void EmitCongestedIntervalTraceEvent(CongestionType congestion_type,
                                               base::TimeTicks start_time,
                                               base::TimeTicks end_time);

  // Exposed for testing.
  base::TimeTicks GetLastCalculationTime();
  StartupStage GetStartupStageForTesting() const;

 private:
  // If sufficient time has passed since the last calculation, then calculate
  // responsiveness again and update |last_calculation_time_|.
  //
  // We only trigger this from the main thread since triggering it from
  // secondary threads would require us to grab the lock, which could cause
  // contention. We only need this to trigger every 30s or so, and we generally
  // expect there to be some activity on the main thread if Chrome is actually
  // in use.
  void CalculateResponsivenessIfNecessary(base::TimeTicks current_time);

  // Responsiveness is calculated by:
  //   1) Discretizing time into small intervals.
  //   2) In each interval, looking to see if there is a Congestion. If so, the
  //   interval is marked as |congested|.
  //   3) Computing the percentage of intervals that are congested.
  void CalculateResponsiveness(
      CongestionType congestion_type,
      const std::vector<SourceCongestions>& congestions_from_multiple_sources,
      base::TimeTicks start_time,
      base::TimeTicks end_time);

  void EmitResponsivenessHistograms(std::string_view base_histogram_name,
                                    CongestionType congestion_type,
                                    size_t num_congested_slices,
                                    bool notify_delegate);

#if BUILDFLAG(IS_ANDROID)
  // Callback invoked when the application state changes.
  void OnApplicationStateChanged(base::android::ApplicationState state);
#endif

  // This helper method:
  //   1) Removes all Congestions with Congestion.end_time < |end_time| from
  //   |congestions|. 2) Returns all Congestions with Congestion.start_time <
  //   |end_time|.
  static CongestionList TakeCongestionsOlderThanTime(
      CongestionList* congestions,
      base::TimeTicks end_time);

  const std::string histogram_prefix_;
  const bool consume_secondary_sources_;

  SEQUENCE_CHECKER(main_thread_sequence_checker_);

  // Congestion from tasks/events on the main thread.
  SourceCongestions main_thread_source_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  // Stores the current visibility state of the application.
  bool is_application_visible_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_) = false;
#endif

  StartupStage startup_stage_ GUARDED_BY_CONTEXT(
      main_thread_sequence_checker_) = StartupStage::kFirstInterval;
  bool past_first_idle_ GUARDED_BY_CONTEXT(main_thread_sequence_checker_) =
      false;

  // The last time at which metrics were emitted. All congestions older than
  // this time have been consumed. Newer congestions are still in their
  // CongestionLists waiting to be consumed.
  base::TimeTicks last_calculation_time_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

  // This class keeps track of the time at which any activity occurred on the
  // main thread. If a sufficiently long period of time passes without any
  // activity, then it's assumed that the process was suspended. In this case,
  // we should not emit any responsiveness metrics.
  //
  // Note that the process may be suspended while a task or event is being
  // executed, so a very long execution time should be treated similarly.
  base::TimeTicks most_recent_activity_time_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

  // Used to record embedder-specific responsiveness metrics.
  std::unique_ptr<ResponsivenessCalculatorDelegate> delegate_
      GUARDED_BY_CONTEXT(main_thread_sequence_checker_);

  const perfetto::NamedTrack congestion_track_;

#if BUILDFLAG(IS_ANDROID)
  // Listener for changes in application state, unregisters itself when
  // destroyed.
  std::unique_ptr<base::android::ApplicationStatusListener>
      application_status_listener_
          GUARDED_BY_CONTEXT(main_thread_sequence_checker_);
#endif
};

// Process-wide registry of secondary sources reporting responsiveness
// congestions, modeled on `base::HangWatcher`. In production a leaked instance
// is created via `Watcher::EnableForProcess()`; unit tests instantiate a
// `SecondarySources` scoped to the test (outliving any attached consumer and
// secondary-thread registrations).
class BASE_EXPORT SecondarySources {
 public:
  using SourceId = base::IdType<SecondarySources, size_t, 0>;

  SecondarySources();
  SecondarySources(const SecondarySources&) = delete;
  SecondarySources& operator=(const SecondarySources&) = delete;
  ~SecondarySources();

  // Returns the active process-wide registry, or nullptr if none exists.
  static SecondarySources* Get();

  SourceId RegisterSource(std::string_view source_name = "");
  void RecordTaskOrEvent(SourceId source_id,
                         base::TimeTicks queue_time,
                         base::TimeTicks execution_start_time,
                         base::TimeTicks execution_finish_time);

  void TakeCongestionsOlderThanTime(
      std::vector<Calculator::SourceCongestions>& out,
      base::TimeTicks end_time);

 private:
  friend class Calculator;

  void AttachConsumer();
  void DetachConsumer();
  void Clear();

  // Low contention: acquired by secondary threads only when
  // `execution_finish_time - queue_time >= 100ms` (plus registration), and by
  // the main thread every 30s (or on suspend / attach / detach).
  base::Lock lock_;
  // Marks whether a `Calculator` is consuming this registry (at most one at a
  // time). While unattached, `RecordTaskOrEvent()` drops congestions.
  bool has_consumer_ GUARDED_BY(lock_) = false;
  std::vector<Calculator::SourceCongestions> sources_ GUARDED_BY(lock_);
};

}  // namespace responsiveness
}  // namespace base

#endif  // BASE_TASK_RESPONSIVENESS_CALCULATOR_H_
