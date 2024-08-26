// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller.h"

#include <atomic>
#include <string_view>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace sequence_manager {
namespace internal {

namespace {
// Enable sample metadata recording in this class, if it's currently disabled.
// Note that even if `kThreadControllerSetsProfilerMetadata` is disabled, sample
// metadata may still be recorded.
BASE_FEATURE(kThreadControllerSetsProfilerMetadata,
             "ThreadControllerSetsProfilerMetadata",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Thread safe copy to be updated once feature list is available. This
// defaults to true to make sure that no metadata is lost on clients that
// need to record. This leads to some overeporting before feature list
// initialization on other clients but that's still way better than the current
// situation which is reporting all the time.
std::atomic<bool> g_thread_controller_sets_profiler_metadata{true};

// ThreadController interval metrics are mostly of interest for intervals that
// are not trivially short. Under a certain threshold it's unlikely that
// intervention from developers would move metrics. Log with suffix for
// intervals under a threshold chosen via tracing data. To validate the
// threshold makes sense and does not filter out too many samples
// ThreadController.ActiveIntervalDuration can be used.
constexpr TimeDelta kNonTrivialActiveIntervalLength = Milliseconds(1);
constexpr TimeDelta kMediumActiveIntervalLength = Milliseconds(100);

std::string MakeSuffix(std::string_view time_suffix,
                       std::string_view thread_name) {
  return base::StrCat({".", time_suffix, ".", thread_name});
}

}  // namespace

ThreadController::ThreadController(const TickClock* time_source)
    : associated_thread_(AssociatedThreadId::CreateUnbound()),
      time_source_(time_source) {}

ThreadController::~ThreadController() = default;

void ThreadController::SetTickClock(const TickClock* clock) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  time_source_ = clock;
}

ThreadController::RunLevelTracker::RunLevelTracker(
    const ThreadController& outer)
    : outer_(outer) {}

ThreadController::RunLevelTracker::~RunLevelTracker() {
  DCHECK_CALLED_ON_VALID_THREAD(outer_->associated_thread_->thread_checker);

  // There shouldn't be any remaining |run_levels_| by the time this unwinds.
  DCHECK_EQ(run_levels_.size(), 0u);
}

// static
void ThreadController::InitializeFeatures(
    features::EmitThreadControllerProfilerMetadata emit_profiler_metadata) {
  g_thread_controller_sets_profiler_metadata.store(
      emit_profiler_metadata ==
              features::EmitThreadControllerProfilerMetadata::kForce ||
          base::FeatureList::IsEnabled(kThreadControllerSetsProfilerMetadata),
      std::memory_order_relaxed);
}

bool ThreadController::RunLevelTracker::RunLevel::ShouldRecordSampleMetadata() {
  return g_thread_controller_sets_profiler_metadata.load(
      std::memory_order_relaxed);
}

std::string_view ThreadController::RunLevelTracker::RunLevel::GetThreadName() {
  std::string_view thread_name = "Other";
  if (!time_keeper_->thread_name().empty()) {
    thread_name = time_keeper_->thread_name();
  }
  return thread_name;
}

std::string
ThreadController::RunLevelTracker::RunLevel::GetSuffixForCatchAllHistogram() {
  return MakeSuffix("Any", GetThreadName());
}

std::string ThreadController::RunLevelTracker::RunLevel::GetSuffixForHistogram(
    TimeDelta duration) {
  std::string_view time_suffix;
  if (duration < kNonTrivialActiveIntervalLength) {
    time_suffix = "Short";
  } else if (duration < kMediumActiveIntervalLength) {
    time_suffix = "Medium";
  }
  return MakeSuffix(time_suffix, GetThreadName());
}

void ThreadController::EnableMessagePumpTimeKeeperMetrics(
    const char* thread_name,
    bool wall_time_based_metrics_enabled_for_testing) {
  // MessagePump runs too fast, a low-res clock would result in noisy metrics.
  if (!base::TimeTicks::IsHighResolution())
    return;

  run_level_tracker_.EnableTimeKeeperMetrics(
      thread_name, wall_time_based_metrics_enabled_for_testing);
}

void ThreadController::RunLevelTracker::EnableTimeKeeperMetrics(
    const char* thread_name,
    bool wall_time_based_metrics_enabled_for_testing) {
  time_keeper_.EnableRecording(thread_name,
                               wall_time_based_metrics_enabled_for_testing);
}

void ThreadController::RunLevelTracker::TimeKeeper::EnableRecording(
    const char* thread_name,
    bool wall_time_based_metrics_enabled_for_testing) {
  DCHECK(!histogram_);
  thread_name_ = thread_name;
  wall_time_based_metrics_enabled_for_testing_ =
      wall_time_based_metrics_enabled_for_testing;

  histogram_ = LinearHistogram::FactoryGet(
      JoinString({"Scheduling.MessagePumpTimeKeeper", thread_name}, "."), 1,
      Phase::kLastPhase, Phase::kLastPhase + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);

#if BUILDFLAG(ENABLE_BASE_TRACING)
  perfetto_track_.emplace(
      reinterpret_cast<uint64_t>(this),
      // TODO(crbug.com/42050015): Replace with ThreadTrack::Current() after SDK
      // migration.
      // In the non-SDK version, ThreadTrack::Current() returns a different
      // track id on some platforms (for example Mac OS), which results in
      // async tracks not being associated with their thread.
      perfetto::ThreadTrack::ForThread(base::PlatformThread::CurrentId()));
  // TODO(crbug.com/42050015): Use Perfetto library to name this Track.
  // auto desc = perfetto_track_->Serialize();
  // desc.set_name(JoinString({"MessagePumpPhases", thread_name}, " "));
  // perfetto::internal::TrackEventDataSource::SetTrackDescriptor(
  //     *perfetto_track_, desc);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

void ThreadController::RunLevelTracker::OnRunLoopStarted(State initial_state,
                                                         LazyNow& lazy_now) {
  DCHECK_CALLED_ON_VALID_THREAD(outer_->associated_thread_->thread_checker);

  const bool is_nested = !run_levels_.empty();
  run_levels_.emplace(initial_state, is_nested, time_keeper_, lazy_now);

  // In unit tests, RunLoop::Run() acts as the initial wake-up.
  if (!is_nested && initial_state != kIdle)
    time_keeper_.RecordWakeUp(lazy_now);
}

void ThreadController::RunLevelTracker::OnRunLoopEnded() {
  DCHECK_CALLED_ON_VALID_THREAD(outer_->associated_thread_->thread_checker);
  // Normally this will occur while kIdle or kInBetweenWorkItems but it can also
  // occur while kRunningWorkItem in rare situations where the owning
  // ThreadController is deleted from within a task. Ref.
  // SequenceManagerWithTaskRunnerTest.DeleteSequenceManagerInsideATask. Thus we
  // can't assert anything about the current state other than that it must be
  // exiting an existing RunLevel.
  DCHECK(!run_levels_.empty());
  LazyNow exit_lazy_now(outer_->time_source_);
  run_levels_.top().set_exit_lazy_now(&exit_lazy_now);
  run_levels_.pop();
}

void ThreadController::RunLevelTracker::OnWorkStarted(LazyNow& lazy_now) {
  DCHECK_CALLED_ON_VALID_THREAD(outer_->associated_thread_->thread_checker);
  // Ignore work outside the main run loop.
  // The only practical case where this would happen is if a native loop is spun
  // outside the main runloop (e.g. system dialog during startup). We cannot
  // support this because we are not guaranteed to be able to observe its exit
  // (like we would inside an application task which is at least guaranteed to
  // itself notify us when it ends). Some ThreadControllerWithMessagePumpTest
  // also drive ThreadController outside a RunLoop and hit this.
  if (run_levels_.empty())
    return;

  // Already running a work item? => #work-in-work-implies-nested
  if (run_levels_.top().state() == kRunningWorkItem) {
    run_levels_.emplace(kRunningWorkItem, /*nested=*/true, time_keeper_,
                        lazy_now);
  } else {
    if (run_levels_.top().state() == kIdle) {
      time_keeper_.RecordWakeUp(lazy_now);
    } else {
      time_keeper_.RecordEndOfPhase(kPumpOverhead, lazy_now);
    }

    // Going from kIdle or kInBetweenWorkItems to kRunningWorkItem.
    run_levels_.top().UpdateState(kRunningWorkItem, lazy_now);
  }
}

void ThreadController::RunLevelTracker::OnApplicationTaskSelected(
    TimeTicks queue_time,
    LazyNow& lazy_now) {
  DCHECK_CALLED_ON_VALID_THREAD(outer_->associated_thread_->thread_checker);
  // As-in OnWorkStarted. Early native loops can result in
  // ThreadController::DoWork because the lack of a top-level RunLoop means
  // `task_execution_allowed` wasn't consumed.
  if (run_levels_.empty())
    return;

  // OnWorkStarted() is expected to precede OnApplicationTaskSelected().
  DCHECK_EQ(run_levels_.top().state(), kRunningWorkItem);

  time_keeper_.OnApplicationTaskSelected(queue_time, lazy_now);
}

void ThreadController::RunLevelTracker::OnWorkEnded(LazyNow& lazy_now,
                                                    int run_level_depth) {
  DCHECK_CALLED_ON_VALID_THREAD(outer_->associated_thread_->thread_checker);
  if (run_levels_.empty())
    return;

  // #done-work-at-lower-runlevel-implies-done-nested
  if (run_level_depth != static_cast<int>(num_run_levels())) {
    DCHECK_EQ(run_level_depth + 1, static_cast<int>(num_run_levels()));
    run_levels_.top().set_exit_lazy_now(&lazy_now);
    run_levels_.pop();
  } else {
    time_keeper_.RecordEndOfPhase(kWorkItem, lazy_now);
  }

  // Whether we exited a nested run-level or not: the current run-level is now
  // transitioning from kRunningWorkItem to kInBetweenWorkItems.
  DCHECK_EQ(run_levels_.top().state(), kRunningWorkItem);
  run_levels_.top().UpdateState(kInBetweenWorkItems, lazy_now);
}

void ThreadController::RunLevelTracker::OnIdle(LazyNow& lazy_now) {
  DCHECK_CALLED_ON_VALID_THREAD(outer_->associated_thread_->thread_checker);
  if (run_levels_.empty())
    return;

  DCHECK_NE(run_levels_.top().state(), kRunningWorkItem);
  time_keeper_.RecordEndOfPhase(kIdleWork, lazy_now);
  run_levels_.top().UpdateState(kIdle, lazy_now);
}

void ThreadController::RunLevelTracker::RecordScheduleWork() {
  // Matching TerminatingFlow is found at
  // ThreadController::RunLevelTracker::RunLevel::UpdateState
  if (outer_->associated_thread_->IsBoundToCurrentThread()) {
    TRACE_EVENT_INSTANT("wakeup.flow", "ScheduleWorkToSelf");
  } else {
    TRACE_EVENT_INSTANT("wakeup.flow", "ScheduleWork",
                        perfetto::Flow::FromPointer(this));
  }
}

// static
void ThreadController::RunLevelTracker::SetTraceObserverForTesting(
    TraceObserverForTesting* trace_observer_for_testing) {
  DCHECK_NE(!!trace_observer_for_testing_, !!trace_observer_for_testing);
  trace_observer_for_testing_ = trace_observer_for_testing;
}

// static
ThreadController::RunLevelTracker::TraceObserverForTesting*
    ThreadController::RunLevelTracker::trace_observer_for_testing_ = nullptr;

ThreadController::RunLevelTracker::RunLevel::RunLevel(State initial_state,
                                                      bool is_nested,
                                                      TimeKeeper& time_keeper,
                                                      LazyNow& lazy_now)
    : is_nested_(is_nested),
      time_keeper_(time_keeper),
      thread_controller_sample_metadata_("ThreadController active",
                                         base::SampleMetadataScope::kThread) {
  if (is_nested_) {
    // Stop the current kWorkItem phase now, it will resume after the kNested
    // phase ends.
    time_keeper_->RecordEndOfPhase(kWorkItemSuspendedOnNested, lazy_now);
  }
  UpdateState(initial_state, lazy_now);
}

ThreadController::RunLevelTracker::RunLevel::~RunLevel() {
  if (!was_moved_) {
    DCHECK(exit_lazy_now_);
    UpdateState(kIdle, *exit_lazy_now_);
    if (is_nested_) {
      // Attribute the entire time in this nested RunLevel to kNested phase. If
      // this wasn't the last nested RunLevel, this is ignored and will be
      // applied on the final pop().
      time_keeper_->RecordEndOfPhase(kNested, *exit_lazy_now_);

      if (ShouldRecordSampleMetadata()) {
        // Intentionally ordered after UpdateState(kIdle), reinstantiates
        // thread_controller_sample_metadata_ when yielding back to a parent
        // RunLevel (which is active by definition as it is currently running
        // this one).
        thread_controller_sample_metadata_.Set(
            static_cast<int64_t>(++thread_controller_active_id_));
      }
    }
  }
}

ThreadController::RunLevelTracker::RunLevel::RunLevel(RunLevel&& other) =
    default;

void ThreadController::RunLevelTracker::RunLevel::LogPercentageMetric(
    const char* name,
    int percentage) {
  UmaHistogramPercentage(base::StrCat({name, ".", GetThreadName()}),
                         percentage);
}

void ThreadController::RunLevelTracker::RunLevel::LogPercentageMetric(
    const char* name,
    int percentage,
    base::TimeDelta interval_duration) {
  UmaHistogramPercentage(base::StrCat({name, GetSuffixForCatchAllHistogram()}),
                         percentage);
  UmaHistogramPercentage(
      base::StrCat({name, GetSuffixForHistogram(interval_duration)}),
      percentage);
}

void ThreadController::RunLevelTracker::RunLevel::LogIntervalMetric(
    const char* name,
    base::TimeDelta value,
    base::TimeDelta interval_duration) {
  // Log towards "Any" time suffix first.
  UmaHistogramTimes(base::StrCat({name, GetSuffixForCatchAllHistogram()}),
                    value);
  if (interval_duration < kNonTrivialActiveIntervalLength) {
    UmaHistogramCustomMicrosecondsTimes(
        base::StrCat({name, GetSuffixForHistogram(interval_duration)}), value,
        base::Microseconds(1), kNonTrivialActiveIntervalLength, 100);
  } else if (interval_duration < kMediumActiveIntervalLength) {
    UmaHistogramCustomTimes(
        base::StrCat({name, GetSuffixForHistogram(interval_duration)}), value,
        kNonTrivialActiveIntervalLength, kMediumActiveIntervalLength, 100);
  }
}

void ThreadController::RunLevelTracker::RunLevel::LogOnActiveMetrics(
    LazyNow& lazy_now) {
  CHECK(last_active_start_.is_null());
  CHECK(last_active_threadtick_start_.is_null());

  if (!last_active_end_.is_null()) {
    const base::TimeDelta idle_time = lazy_now.Now() - last_active_end_;
    LogIntervalMetric("Scheduling.ThreadController.IdleDuration", idle_time,
                      idle_time);
    last_active_end_ = base::TimeTicks();
    accumulated_idle_time_ += idle_time;
  }

  // Taking thread ticks can be expensive. Make sure to do it rarely enough to
  // not have a discernible impact on performance.
  static const bool thread_ticks_supported = ThreadTicks::IsSupported();
  // Disable subsampling to support wall-time based metrics. Only supported for
  // testing purposes. By default, the subsampling probability is 0.1%.
  const double probability =
      time_keeper_->wall_time_based_metrics_enabled_for_testing() ? 1.0 : 0.001;
  if (thread_ticks_supported &&
      metrics_sub_sampler_.ShouldSample(probability)) {
    last_active_start_ = lazy_now.Now();
    last_active_threadtick_start_ = ThreadTicks::Now();
  }
}

void ThreadController::RunLevelTracker::RunLevel::LogOnIdleMetrics(
    LazyNow& lazy_now) {
  if (!last_active_start_.is_null()) {
    const base::TimeDelta elapsed_ticks = lazy_now.Now() - last_active_start_;
    base::TimeDelta elapsed_thread_ticks =
        ThreadTicks::Now() - last_active_threadtick_start_;

    // Round to 100% in case of clock imprecisions making it look like
    // there's impossibly more ThreadTicks than TimeTicks elapsed.
    elapsed_thread_ticks = std::min(elapsed_thread_ticks, elapsed_ticks);

    LogIntervalMetric("Scheduling.ThreadController.ActiveIntervalDuration",
                      elapsed_ticks, elapsed_ticks);
    LogIntervalMetric(
        "Scheduling.ThreadController.ActiveIntervalOffCpuDuration",
        elapsed_ticks - elapsed_thread_ticks, elapsed_ticks);
    LogIntervalMetric("Scheduling.ThreadController.ActiveIntervalOnCpuDuration",
                      elapsed_thread_ticks, elapsed_ticks);

    // If the interval was shorter than a tick, 100% on-cpu time is assumed.
    int active_interval_cpu_percentage =
        elapsed_ticks.is_zero()
            ? 100
            : static_cast<int>(
                  (elapsed_thread_ticks * 100).IntDiv(elapsed_ticks));

    LogPercentageMetric(
        "Scheduling.ThreadController.ActiveIntervalOnCpuPercentage",
        active_interval_cpu_percentage, elapsed_ticks);

    if (time_keeper_->wall_time_based_metrics_enabled_for_testing()) {
      accumulated_active_time_ += elapsed_ticks;
      accumulated_active_on_cpu_time_ += elapsed_thread_ticks;
      accumulated_active_off_cpu_time_ +=
          (elapsed_ticks - elapsed_thread_ticks);

      // Accumulated wall-time since last wall-time based metric was stored.
      const base::TimeDelta accumulated_wall_time =
          accumulated_active_time_ + accumulated_idle_time_;

      // Add wall-time based ratio metrics (in percent) when the total sum of
      // active and idle times is larger than one second.
      if (accumulated_wall_time > Seconds(1)) {
        const int active_vs_wall_time_percentage = checked_cast<int>(
            (accumulated_active_time_ * 100).IntDiv(accumulated_wall_time));
        LogPercentageMetric(
            "Scheduling.ThreadController.ActiveVsWallTimePercentage",
            active_vs_wall_time_percentage);
        const int active_on_cpu_vs_wall_time_percentage =
            checked_cast<int>((accumulated_active_on_cpu_time_ * 100)
                                  .IntDiv(accumulated_wall_time));
        LogPercentageMetric(
            "Scheduling.ThreadController.ActiveOnCpuVsWallTimePercentage",
            active_on_cpu_vs_wall_time_percentage);
        const int active_off_cpu_vs_wall_time_percentage =
            checked_cast<int>((accumulated_active_off_cpu_time_ * 100)
                                  .IntDiv(accumulated_wall_time));
        LogPercentageMetric(
            "Scheduling.ThreadController.ActiveOffCpuVsWallTimePercentage",
            active_off_cpu_vs_wall_time_percentage);

        accumulated_idle_time_ = base::TimeDelta();
        accumulated_active_time_ = base::TimeDelta();
        accumulated_active_on_cpu_time_ = base::TimeDelta();
        accumulated_active_off_cpu_time_ = base::TimeDelta();
      }
    }

    // Reset timings.
    last_active_start_ = base::TimeTicks();
    last_active_threadtick_start_ = base::ThreadTicks();
    last_active_end_ = lazy_now.Now();
  }
}

void ThreadController::RunLevelTracker::RunLevel::UpdateState(
    State new_state,
    LazyNow& lazy_now) {
  // The only state that can be redeclared is idle, anything else should be a
  // transition.
  DCHECK(state_ != new_state || new_state == kIdle)
      << state_ << "," << new_state;

  const bool was_active = state_ != kIdle;
  const bool is_active = new_state != kIdle;

  state_ = new_state;
  if (was_active == is_active)
    return;

  // Change of state.
  if (is_active) {
    LogOnActiveMetrics(lazy_now);

    // Flow emission is found at
    // ThreadController::RunLevelTracker::RecordScheduleWork.
    TRACE_EVENT_BEGIN("base", "ThreadController active", lazy_now.Now(),
                      [&](perfetto::EventContext& ctx) {
                        time_keeper_->MaybeEmitIncomingWakeupFlow(ctx);
                      });

    if (ShouldRecordSampleMetadata()) {
      // Overriding the annotation from the previous RunLevel is intentional.
      // Only the top RunLevel is ever updated, which holds the relevant state.
      thread_controller_sample_metadata_.Set(
          static_cast<int64_t>(++thread_controller_active_id_));
    }
  } else {
    if (ShouldRecordSampleMetadata()) {
      thread_controller_sample_metadata_.Remove();
    }

    LogOnIdleMetrics(lazy_now);

    TRACE_EVENT_END("base", lazy_now.Now());
  }

  if (trace_observer_for_testing_) {
    if (is_active)
      trace_observer_for_testing_->OnThreadControllerActiveBegin();
    else
      trace_observer_for_testing_->OnThreadControllerActiveEnd();
  }
}

ThreadController::RunLevelTracker::TimeKeeper::TimeKeeper(
    const RunLevelTracker& outer)
    : outer_(outer) {}

void ThreadController::RunLevelTracker::TimeKeeper::RecordWakeUp(
    LazyNow& lazy_now) {
  if (!ShouldRecordNow(ShouldRecordReqs::kOnWakeUp))
    return;

  // Phase::kScheduled will be accounted against `last_wakeup_` in
  // OnTaskSelected, if there's an application task in this work cycle.
  last_wakeup_ = lazy_now.Now();
  // Account the next phase starting from now.
  last_phase_end_ = last_wakeup_;

#if BUILDFLAG(ENABLE_BASE_TRACING)
  // Emit the END of the kScheduled phase right away, this avoids incorrect
  // ordering when kScheduled is later emitted and its END matches the BEGIN of
  // an already emitted phase (tracing's sort is stable and would keep the late
  // END for kScheduled after the earlier BEGIN of the next phase):
  // crbug.com/1333460. As we just woke up, there are no events active at this
  // point (we don't record MessagePumpPhases while nested). In the absence of
  // a kScheduled phase, this unmatched END will be ignored.
  TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("base"), *perfetto_track_,
                  last_wakeup_);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

void ThreadController::RunLevelTracker::TimeKeeper::OnApplicationTaskSelected(
    TimeTicks queue_time,
    LazyNow& lazy_now) {
  if (!ShouldRecordNow())
    return;

  if (!last_wakeup_.is_null()) {
    // `queue_time` can be null on threads that did not
    // `SetAddQueueTimeToTasks(true)`. `queue_time` can also be ahead of
    // `last_wakeup` in racy cases where the first chrome task is enqueued
    // while the pump was already awake (e.g. for native work). Consider the
    // kScheduled phase inexistent in that case.
    if (!queue_time.is_null() && queue_time < last_wakeup_) {
      if (!last_sleep_.is_null() && queue_time < last_sleep_) {
        // Avoid overlapping kScheduled and kIdleWork phases when work is
        // scheduled while going to sleep.
        queue_time = last_sleep_;
      }
      RecordTimeInPhase(kScheduled, queue_time, last_wakeup_);
#if BUILDFLAG(ENABLE_BASE_TRACING)
      // Match the END event which was already emitted by RecordWakeUp().
      TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("base"),
                        perfetto::StaticString(PhaseToEventName(kScheduled)),
                        *perfetto_track_, queue_time);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
    }
    last_wakeup_ = TimeTicks();
  }
  RecordEndOfPhase(kSelectingApplicationTask, lazy_now);
  current_work_item_is_native_ = false;
}

void ThreadController::RunLevelTracker::TimeKeeper::RecordEndOfPhase(
    Phase phase,
    LazyNow& lazy_now) {
  if (!ShouldRecordNow(phase == kNested ? ShouldRecordReqs::kOnEndNested
                                        : ShouldRecordReqs::kRegular)) {
    return;
  }

  if (phase == kWorkItem && !current_work_item_is_native_) {
    phase = kApplicationTask;
    // Back to assuming future work is native until OnApplicationTaskSelected()
    // is invoked.
    current_work_item_is_native_ = true;
  } else if (phase == kWorkItemSuspendedOnNested) {
    // kWorkItemSuspendedOnNested temporarily marks the end of time allocated to
    // the current work item. It is reported as a separate phase to skip the
    // above `current_work_item_is_native_ = true` which assumes the work item
    // is truly complete.
    phase = current_work_item_is_native_ ? kNativeWork : kApplicationTask;
  }

  const TimeTicks phase_end = lazy_now.Now();
  RecordTimeInPhase(phase, last_phase_end_, phase_end);

#if BUILDFLAG(ENABLE_BASE_TRACING)
  // Ugly hack to name our `perfetto_track_`.
  bool is_tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("base"),
                                     &is_tracing_enabled);
  if (is_tracing_enabled) {
    if (!was_tracing_enabled_) {
      // The first event name on the track hackily names the track...
      // TODO(crbug.com/42050015): Use the Perfetto library to properly name
      // this Track in EnableRecording above.
      TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("base"),
                          "MessagePumpPhases", *perfetto_track_,
                          last_phase_end_ - Seconds(1));
    }

    const char* event_name = PhaseToEventName(phase);
    TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("base"),
                      perfetto::StaticString(event_name), *perfetto_track_,
                      last_phase_end_);
    TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("base"), *perfetto_track_,
                    phase_end);
  }
  was_tracing_enabled_ = is_tracing_enabled;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

  last_phase_end_ = phase_end;
}

void ThreadController::RunLevelTracker::TimeKeeper::MaybeEmitIncomingWakeupFlow(
    perfetto::EventContext& ctx) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  static const uint8_t* flow_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("wakeup.flow");
  if (!*flow_enabled) {
    return;
  }

  perfetto::TerminatingFlow::ProcessScoped(
      reinterpret_cast<uint64_t>(&(outer_.get())))(ctx);
#endif
}

bool ThreadController::RunLevelTracker::TimeKeeper::ShouldRecordNow(
    ShouldRecordReqs reqs) {
  DCHECK_CALLED_ON_VALID_THREAD(
      outer_->outer_->associated_thread_->thread_checker);
  // Recording is technically enabled once `histogram_` is set, however
  // `last_phase_end_` will be null until the next RecordWakeUp in the work
  // cycle in which `histogram_` is enabled. Only start recording from there.
  // Ignore any nested phases. `reqs` may indicate exceptions to this.
  //
  // TODO(crbug.com/40226913): In a follow-up, we could probably always be
  // tracking the phases of the pump and merely ignore the reporting if
  // `histogram_` isn't set.
  switch (reqs) {
    case ShouldRecordReqs::kRegular:
      return histogram_ && !last_phase_end_.is_null() &&
             outer_->run_levels_.size() == 1;
    case ShouldRecordReqs::kOnWakeUp:
      return histogram_ && outer_->run_levels_.size() == 1;
    case ShouldRecordReqs::kOnEndNested:
      return histogram_ && !last_phase_end_.is_null() &&
             outer_->run_levels_.size() <= 2;
  }
}

void ThreadController::RunLevelTracker::TimeKeeper::RecordTimeInPhase(
    Phase phase,
    TimeTicks phase_begin,
    TimeTicks phase_end) {
  DCHECK(ShouldRecordNow(phase == kNested ? ShouldRecordReqs::kOnEndNested
                                          : ShouldRecordReqs::kRegular));

  // Report a phase only when at least 100ms has been attributed to it.
  static constexpr auto kReportInterval = Milliseconds(100);

  // Above 30s in a single phase, assume suspend-resume and ignore the report.
  static constexpr auto kSkippedDelta = Seconds(30);

  const auto delta = phase_end - phase_begin;
  DCHECK(!delta.is_negative()) << delta;
  if (delta >= kSkippedDelta)
    return;

  deltas_[phase] += delta;
  if (deltas_[phase] >= kReportInterval) {
    const int count = deltas_[phase] / Milliseconds(1);
    histogram_->AddCount(phase, count);
    deltas_[phase] -= Milliseconds(count);
  }

  if (phase == kIdleWork)
    last_sleep_ = phase_end;

  if (outer_->trace_observer_for_testing_)
    outer_->trace_observer_for_testing_->OnPhaseRecorded(phase);
}

// static
const char* ThreadController::RunLevelTracker::TimeKeeper::PhaseToEventName(
    Phase phase) {
  switch (phase) {
    case kScheduled:
      return "Scheduled";
    case kPumpOverhead:
      return "PumpOverhead";
    case kNativeWork:
      return "NativeTask";
    case kSelectingApplicationTask:
      return "SelectingApplicationTask";
    case kApplicationTask:
      return "ApplicationTask";
    case kIdleWork:
      return "IdleWork";
    case kNested:
      return "Nested";
    case kWorkItemSuspendedOnNested:
      // kWorkItemSuspendedOnNested should be transformed into kNativeWork or
      // kApplicationTask before this point.
      NOTREACHED();
  }
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
