// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/responsiveness/calculator.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/trace_event/histogram_scope.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "build/build_config.h"

namespace base {
namespace responsiveness {

namespace {

SecondarySources* g_secondary_sources = nullptr;

// We divide the measurement interval into discretized time slices. Each slice
// is marked as congested if it contained a congested task. A congested task is
// one whose execution latency is greater than kCongestionThreshold.
constexpr auto kMeasurementPeriod = base::Seconds(30);

// A task or event longer than kCongestionThreshold is considered congested.
constexpr auto kCongestionThreshold = base::Milliseconds(100);

// If there have been no events/tasks on the main thread for a significant
// period of time, it's likely because Chrome was suspended.
// This value is copied from queueing_time_estimator.cc:kInvalidPeriodThreshold.
constexpr auto kSuspendInterval = base::Seconds(30);

constexpr char kLatencyEventCategory[] = "latency";

// The names emitted for CongestedIntervals measurement events.
constexpr char kCongestionTrack[] = "MainThreadsCongestion";

perfetto::StaticString GetCongestedIntervalEvent(
    Calculator::CongestionType congestion_type) {
  switch (congestion_type) {
    case Calculator::CongestionType::kExecutionOnly:
      return "CongestedInterval.RunningOnly";
    case Calculator::CongestionType::kQueueAndExecution:
      return "CongestedInterval";
  }
}

perfetto::StaticString GetCongestedIntervalsMeasurementEvent(
    Calculator::StartupStage startup_stage) {
  switch (startup_stage) {
    case Calculator::StartupStage::kFirstInterval:
    case Calculator::StartupStage::kFirstIntervalDoneWithoutFirstIdle:
    case Calculator::StartupStage::kFirstIntervalAfterFirstIdle:
      return "MainThreadsCongestion.Initial";
    case Calculator::StartupStage::kPeriodic:
      return "MainThreadsCongestion.Periodic";
  }
}

// Given a |congestion|, finds each congested slice between |start_time| and
// |end_time|, and adds it to |congested_slices|.
void AddCongestedSlices(std::set<int>* congested_slices,
                        const Calculator::Congestion& congestion,
                        base::TimeTicks start_time,
                        base::TimeTicks end_time) {
  // Ignore the first congestion threshold, since that's the part of the
  // task/event that wasn't congested.
  base::TimeTicks congestion_start =
      congestion.start_time + kCongestionThreshold;

  // Bound by |start_time| and |end_time|.
  congestion_start = std::max(congestion_start, start_time);
  base::TimeTicks congestion_end = std::min(congestion.end_time, end_time);

  // Find each congested slice, and add it to |congested_slices|.
  while (congestion_start < congestion_end) {
    // Convert |congestion_start| to a slice label.
    int64_t label =
        (congestion_start - start_time).IntDiv(kCongestionThreshold);
    congested_slices->insert(checked_cast<int>(label));

    congestion_start += kCongestionThreshold;
  }
}

void RecordCongestion(Calculator::SourceCongestions& source,
                      base::TimeTicks queue_time,
                      base::TimeTicks execution_start_time,
                      base::TimeTicks execution_finish_time) {
  if (execution_finish_time - queue_time >= kCongestionThreshold) {
    source.congestion.emplace_back(queue_time, execution_finish_time);
    if (execution_finish_time - execution_start_time >= kCongestionThreshold) {
      source.execution_congestion.emplace_back(execution_start_time,
                                               execution_finish_time);
    }
  }
}

}  // namespace

Calculator::Congestion::Congestion(base::TimeTicks start_time,
                                   base::TimeTicks end_time)
    : start_time(start_time), end_time(end_time) {
  CHECK_LE(start_time, end_time, base::NotFatalUntil::M160);
}

Calculator::SourceCongestions::SourceCongestions(std::string_view name)
    : name(name) {}
Calculator::SourceCongestions::SourceCongestions(
    std::string name,
    CongestionList execution_congestion,
    CongestionList congestion)
    : name(std::move(name)),
      execution_congestion(std::move(execution_congestion)),
      congestion(std::move(congestion)) {}
Calculator::SourceCongestions::SourceCongestions(SourceCongestions&&) = default;
Calculator::SourceCongestions& Calculator::SourceCongestions::operator=(
    SourceCongestions&&) = default;
Calculator::SourceCongestions::~SourceCongestions() = default;

Calculator::SourceCongestions Calculator::SourceCongestions::TakeOlderThanTime(
    base::TimeTicks end_time) {
  return SourceCongestions(
      name,
      Calculator::TakeCongestionsOlderThanTime(&execution_congestion, end_time),
      Calculator::TakeCongestionsOlderThanTime(&congestion, end_time));
}

void Calculator::SourceCongestions::Clear() {
  execution_congestion.clear();
  congestion.clear();
}

Calculator::Calculator(
    std::unique_ptr<ResponsivenessCalculatorDelegate> delegate,
    std::string_view histogram_prefix)
    : Calculator(std::move(delegate), histogram_prefix, Config{}) {}

Calculator::Calculator(
    std::unique_ptr<ResponsivenessCalculatorDelegate> delegate,
    std::string_view histogram_prefix,
    Config config)
    : histogram_prefix_(histogram_prefix),
      consume_secondary_sources_(config.consume_secondary_sources),
      main_thread_source_(config.main_source_name),
      last_calculation_time_(base::TimeTicks::Now()),
      most_recent_activity_time_(last_calculation_time_),
      delegate_(std::move(delegate)),
      congestion_track_(
          kCongestionTrack,
          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this))) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  if (consume_secondary_sources_) {
    SecondarySources::Get()->AttachConsumer();
  }
#if BUILDFLAG(IS_ANDROID)
  if (config.observe_application_status) {
    application_status_listener_ =
        base::android::ApplicationStatusListener::New(
            base::BindRepeating(&Calculator::OnApplicationStateChanged,
                                // Listener is destroyed at destructor, and
                                // object will be alive for any callback.
                                base::Unretained(this)));
    OnApplicationStateChanged(
        base::android::ApplicationStatusListener::GetState());
  }
#endif
}

Calculator::~Calculator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  if (consume_secondary_sources_) {
    SecondarySources::Get()->DetachConsumer();
  }
}

void Calculator::TaskOrEventFinishedOnMainThread(
    base::TimeTicks queue_time,
    base::TimeTicks execution_start_time,
    base::TimeTicks execution_finish_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  // TODO(crbug.com/565336492): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK_GE(execution_start_time, queue_time);

  RecordCongestion(main_thread_source_, queue_time, execution_start_time,
                   execution_finish_time);

  // We rely on the assumption that |finish_time| is the current time.
  CalculateResponsivenessIfNecessary(/*current_time=*/execution_finish_time);
}

void Calculator::OnFirstIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  CHECK(!past_first_idle_, base::NotFatalUntil::M160);
  past_first_idle_ = true;
}

void Calculator::EmitResponsiveness(CongestionType congestion_type,
                                    size_t num_congested_slices,
                                    uint64_t event_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  base::trace_event::HistogramScope scoped_event(event_id);
  const std::string base_histogram_name =
      StrCat({histogram_prefix_, ".MainThreadsCongestion"});
  EmitResponsivenessHistograms(base_histogram_name, congestion_type,
                               num_congested_slices,
                               /*notify_delegate=*/true);
}

void Calculator::EmitResponsivenessHistograms(
    std::string_view base_histogram_name,
    CongestionType congestion_type,
    size_t num_congested_slices,
    bool notify_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  static constexpr size_t kMaxCongestedSlices =
      kMeasurementPeriod / kCongestionThreshold;
  static constexpr int kMaxCongestedSlicesInt =
      checked_cast<int>(kMaxCongestedSlices);
  static constexpr size_t kBucketCount = 50;
  CHECK_LE(num_congested_slices, kMaxCongestedSlices,
           base::NotFatalUntil::M160);
  const int sample = checked_cast<int>(num_congested_slices);
  switch (congestion_type) {
    case CongestionType::kExecutionOnly: {
      base::UmaHistogramCustomCounts(
          StrCat({base_histogram_name, ".RunningOnly"}), sample, 1, 1000,
          kBucketCount);
      // Only kFirstInterval and kPeriodic are reported with a suffix, stages
      // in between are only part of the unsuffixed histogram.
      if (startup_stage_ == StartupStage::kFirstInterval) {
        base::UmaHistogramCustomCounts(
            StrCat({base_histogram_name, ".RunningOnly.Initial"}), sample, 1,
            1000, kBucketCount);
      } else if (startup_stage_ == StartupStage::kPeriodic) {
        base::UmaHistogramCustomCounts(
            StrCat({base_histogram_name, ".RunningOnly.Periodic"}), sample, 1,
            1000, kBucketCount);
      }
      break;
    }
    case CongestionType::kQueueAndExecution: {
      // Queuing congestion doesn't count before OnFirstIdle().
      if (startup_stage_ == StartupStage::kFirstInterval ||
          startup_stage_ == StartupStage::kFirstIntervalDoneWithoutFirstIdle) {
        break;
      }
      base::UmaHistogramCustomCounts(base_histogram_name, sample, 1,
                                     kMaxCongestedSlicesInt, kBucketCount);
      if (notify_delegate && delegate_) {
        delegate_->OnResponsivenessEmitted(sample, 1, kMaxCongestedSlicesInt,
                                           kBucketCount);
      }
      if (startup_stage_ == StartupStage::kFirstIntervalAfterFirstIdle) {
        base::UmaHistogramCustomCounts(
            StrCat({base_histogram_name, ".Initial"}), sample, 1,
            kMaxCongestedSlicesInt, kBucketCount);
      } else if (startup_stage_ == StartupStage::kPeriodic) {
        base::UmaHistogramCustomCounts(
            StrCat({base_histogram_name, ".Periodic"}), sample, 1,
            kMaxCongestedSlicesInt, kBucketCount);
      }
      break;
    }
  }
}

void Calculator::EmitResponsivenessTraceEvents(
    CongestionType congestion_type,
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    const std::set<int>& congested_slices,
    uint64_t event_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  // Only output kCongestedIntervalsMeasurementEvent event when there are
  // congested slices during the measurement.
  if (congested_slices.empty()) {
    return;
  }

  // Emit a trace event to highlight the duration of congested intervals
  // measurement.
  if (congestion_type == CongestionType::kQueueAndExecution) {
    EmitCongestedIntervalsMeasurementTraceEvent(
        start_time, end_time, congested_slices.size(), event_id);
    // Since a lot of startup tasks are queue and then released, queuing
    // congestion is very noisy and thus ignored before OnFirstIdle().
    if (startup_stage_ == StartupStage::kFirstInterval ||
        startup_stage_ == StartupStage::kFirstIntervalDoneWithoutFirstIdle) {
      return;
    }
  }

  // |congested_slices| contains the id of congested slices, e.g.
  // {3,6,7,8,41,42}. As such if the slice following slice x is x+1, we coalesce
  // it.
  std::set<int>::const_iterator congested_slice_it = congested_slices.begin();
  while (congested_slice_it != congested_slices.end()) {
    const int start_slice = *congested_slice_it;

    // Find the first slice that is not in the current sequence. After the loop,
    // |congested_slice_it| will point to the first congested slice in the next
    // sequence(or end() if at the end of the slices) while |current_slice|
    // will point to the first non-congested slice number which correspond to
    // the end of the current sequence.
    int current_slice = start_slice;
    do {
      ++congested_slice_it;
      ++current_slice;
    } while (congested_slice_it != congested_slices.end() &&
             *congested_slice_it == current_slice);

    // Output a trace event for the range [start_slice, current_slice[.
    EmitCongestedIntervalTraceEvent(
        congestion_type, start_time + start_slice * kCongestionThreshold,
        start_time + current_slice * kCongestionThreshold);
  }
}

void Calculator::EmitCongestedIntervalsMeasurementTraceEvent(
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    size_t num_congested_slices,
    uint64_t event_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  TRACE_EVENT_BEGIN(kLatencyEventCategory,
                    GetCongestedIntervalsMeasurementEvent(startup_stage_),
                    congestion_track_, start_time);
  TRACE_EVENT_END(kLatencyEventCategory, congestion_track_, end_time,
                  perfetto::Flow::Global(event_id));
}

void Calculator::EmitCongestedIntervalTraceEvent(CongestionType congestion_type,
                                                 base::TimeTicks start_time,
                                                 base::TimeTicks end_time) {
  TRACE_EVENT_BEGIN(kLatencyEventCategory,
                    GetCongestedIntervalEvent(congestion_type),
                    congestion_track_, start_time);
  TRACE_EVENT_END(kLatencyEventCategory, congestion_track_, end_time);
}

base::TimeTicks Calculator::GetLastCalculationTime() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  return last_calculation_time_;
}

Calculator::StartupStage Calculator::GetStartupStageForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  return startup_stage_;
}

void Calculator::CalculateResponsivenessIfNecessary(
    base::TimeTicks current_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  base::TimeTicks last_activity_time = most_recent_activity_time_;
  most_recent_activity_time_ = current_time;

  // We intentionally dump all data if it appears that Chrome was suspended.
  // [e.g. machine is asleep, process is backgrounded on Android]. We don't have
  // an explicit signal for this. Instead, we rely on the assumption that when
  // Chrome is not suspended, there is a steady stream of tasks and events on
  // the main thread. If there's been a significant amount of time since the
  // last activity, then it's likely because Chrome was suspended.
  // TODO(crbug.com/545547273): In child processes where the main thread can be
  // idle for >30s while secondary threads (e.g. Compositor or IO) remain active
  // and congested, secondary-thread congestions in that window are currently
  // discarded when the main thread next runs a task.
  bool is_suspended = current_time - last_activity_time > kSuspendInterval;
#if BUILDFLAG(IS_ANDROID)
  if (application_status_listener_) {
    is_suspended |= !is_application_visible_;
  }
#endif
  if (is_suspended) {
    // Notify the delegate that the interval ended so that it can reset its
    // accumulated data for the current interval.
    if (delegate_) {
      delegate_->OnMeasurementIntervalEnded();
    }
    last_calculation_time_ = current_time;
    main_thread_source_.Clear();
    if (consume_secondary_sources_) {
      SecondarySources::Get()->Clear();
    }
    return;
  }

  base::TimeDelta time_since_last_calculation =
      current_time - last_calculation_time_;
  if (time_since_last_calculation <= kMeasurementPeriod) {
    return;
  }

  // At least |kMeasurementPeriod| time has passed, so we want to move forward
  // |last_calculation_time_| and make measurements based on congestions in that
  // interval.
  const base::TimeTicks new_calculation_time =
      current_time - (time_since_last_calculation % kMeasurementPeriod);

  // Acquire the congestions in the measurement interval from the main and
  // secondary sources.
  std::vector<SourceCongestions> congestions_from_multiple_sources;
  congestions_from_multiple_sources.push_back(
      main_thread_source_.TakeOlderThanTime(new_calculation_time));
  if (consume_secondary_sources_) {
    SecondarySources::Get()->TakeCongestionsOlderThanTime(
        congestions_from_multiple_sources, new_calculation_time);
  }

  if (delegate_) {
    delegate_->OnMeasurementIntervalEnded();
  }

  CalculateResponsiveness(CongestionType::kQueueAndExecution,
                          congestions_from_multiple_sources,
                          last_calculation_time_, new_calculation_time);
  CalculateResponsiveness(CongestionType::kExecutionOnly,
                          congestions_from_multiple_sources,
                          last_calculation_time_, new_calculation_time);

  if (startup_stage_ == StartupStage::kFirstInterval) {
    startup_stage_ = StartupStage::kFirstIntervalDoneWithoutFirstIdle;
  }

  if (startup_stage_ == StartupStage::kFirstIntervalDoneWithoutFirstIdle &&
      past_first_idle_) {
    startup_stage_ = StartupStage::kFirstIntervalAfterFirstIdle;
  } else if (startup_stage_ == StartupStage::kFirstIntervalAfterFirstIdle) {
    startup_stage_ = StartupStage::kPeriodic;
  }

  last_calculation_time_ = new_calculation_time;
}

void Calculator::CalculateResponsiveness(
    CongestionType congestion_type,
    const std::vector<SourceCongestions>& congestions_from_multiple_sources,
    base::TimeTicks start_time,
    base::TimeTicks end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  while (start_time < end_time) {
    const base::TimeTicks current_interval_end_time =
        start_time + kMeasurementPeriod;

    // We divide the current measurement interval into slices. Each slice is
    // given a monotonically increasing label, from 0 to |kNumberOfSlices - 1|.
    // Example [all times in milliseconds since UNIX epoch]:
    //   The measurement interval is [50135, 80135].
    //   The slice [50135, 50235] is labeled 0.
    //   The slice [50235, 50335] is labeled 1.
    //   ...
    //   The slice [80035, 80135] is labeled 299.
    std::set<int> congested_slices;

    for (const SourceCongestions& source : congestions_from_multiple_sources) {
      const CongestionList& congestions =
          congestion_type == CongestionType::kExecutionOnly
              ? source.execution_congestion
              : source.congestion;
      if (!source.name.empty()) {
        std::set<int> source_congested_slices;
        for (const Congestion& congestion : congestions) {
          AddCongestedSlices(&source_congested_slices, congestion, start_time,
                             current_interval_end_time);
        }
        const std::string breakdown_histogram_name =
            StrCat({histogram_prefix_, ".MainThreadsCongestion.", source.name});
        EmitResponsivenessHistograms(breakdown_histogram_name, congestion_type,
                                     source_congested_slices.size(),
                                     /*notify_delegate=*/false);
        congested_slices.insert(source_congested_slices.begin(),
                                source_congested_slices.end());
      } else {
        for (const Congestion& congestion : congestions) {
          AddCongestedSlices(&congested_slices, congestion, start_time,
                             current_interval_end_time);
        }
      }
    }

    uint64_t event_id = base::trace_event::GetNextGlobalTraceId();
    EmitResponsiveness(congestion_type, congested_slices.size(), event_id);

    // If the 'latency' tracing category is enabled and we are ready to observe
    // queuing times (past first idle), emit trace events for the measurement
    // duration and the congested slices.
    bool latency_category_enabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(kLatencyEventCategory,
                                       &latency_category_enabled);
    if (latency_category_enabled) {
      EmitResponsivenessTraceEvents(congestion_type, start_time,
                                    current_interval_end_time, congested_slices,
                                    event_id);
    }

    start_time = current_interval_end_time;
  }
}

#if BUILDFLAG(IS_ANDROID)
void Calculator::OnApplicationStateChanged(
    base::android::ApplicationState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  switch (state) {
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
      // The application is still visible and partially hidden in paused state.
      is_application_visible_ = true;
      break;
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      is_application_visible_ = false;
      break;
    case base::android::APPLICATION_STATE_UNKNOWN:
      break;  // Keep in previous state.
  }
}
#endif

// static
Calculator::CongestionList Calculator::TakeCongestionsOlderThanTime(
    CongestionList* congestions,
    base::TimeTicks end_time) {
  // Find all congestions with Congestion.start_time < |end_time|.
  auto it = std::partition(congestions->begin(), congestions->end(),
                           [&end_time](const Congestion& congestion) {
                             return congestion.start_time < end_time;
                           });

  // Early exit. We don't need to remove any Congestions either, since
  // Congestion.end_time
  // >= Congestion.start_time.
  if (it == congestions->begin()) {
    return CongestionList();
  }

  CongestionList congestions_to_return(congestions->begin(), it);

  // Remove all congestions with Congestion.end_time < |end_time|.
  auto first_congestion_to_keep =
      std::partition(congestions->begin(), congestions->end(),
                     [&end_time](const Congestion& congestion) {
                       return congestion.end_time < end_time;
                     });
  congestions->erase(congestions->begin(), first_congestion_to_keep);
  return congestions_to_return;
}

SecondarySources::SecondarySources() {
  CHECK(!g_secondary_sources);
  g_secondary_sources = this;
}

SecondarySources::~SecondarySources() {
  {
    base::AutoLock lock(lock_);
    CHECK(!has_consumer_);
  }
  CHECK_EQ(g_secondary_sources, this);
  g_secondary_sources = nullptr;
}

// static
SecondarySources* SecondarySources::Get() {
  return g_secondary_sources;
}

void SecondarySources::AttachConsumer() {
  base::AutoLock lock(lock_);
  CHECK(!has_consumer_);
  has_consumer_ = true;
}

void SecondarySources::DetachConsumer() {
  base::AutoLock lock(lock_);
  CHECK(has_consumer_);
  has_consumer_ = false;
  for (Calculator::SourceCongestions& source : sources_) {
    source.Clear();
  }
}

SecondarySources::SourceId SecondarySources::RegisterSource(
    std::string_view source_name) {
  base::AutoLock lock(lock_);
  if (!source_name.empty()) {
    CHECK(!std::ranges::contains(sources_, source_name,
                                 &Calculator::SourceCongestions::name));
  }
  sources_.emplace_back(source_name);
  return SourceId(sources_.size());
}

void SecondarySources::RecordTaskOrEvent(
    SourceId source_id,
    base::TimeTicks queue_time,
    base::TimeTicks execution_start_time,
    base::TimeTicks execution_finish_time) {
  // TODO(crbug.com/565232676): CHECK-exclusion: Convert to a CHECK once we
  // are confident it won't be triggered.
  DCHECK_GE(execution_start_time, queue_time);
  if (execution_finish_time - queue_time < kCongestionThreshold) {
    return;
  }
  base::AutoLock lock(lock_);
  if (!has_consumer_) {
    return;
  }
  CHECK(!source_id.is_null());
  const size_t index = source_id.value() - 1;
  CHECK_LT(index, sources_.size());
  RecordCongestion(sources_[index], queue_time, execution_start_time,
                   execution_finish_time);
}

void SecondarySources::TakeCongestionsOlderThanTime(
    std::vector<Calculator::SourceCongestions>& out,
    base::TimeTicks end_time) {
  base::AutoLock lock(lock_);
  out.reserve(out.size() + sources_.size());
  for (Calculator::SourceCongestions& source : sources_) {
    out.push_back(source.TakeOlderThanTime(end_time));
  }
}

void SecondarySources::Clear() {
  base::AutoLock lock(lock_);
  for (Calculator::SourceCongestions& source : sources_) {
    source.Clear();
  }
}

}  // namespace responsiveness
}  // namespace base
