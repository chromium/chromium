// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/hang_watcher.h"

#include <atomic>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/threading_features.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

namespace {

// Defines how much logging happens when the HangWatcher monitors the threads.
// Logging levels are set per thread type through Finch. It's important that
// the order of the enum members stay the and that their numerical
// values be in increasing order. The implementation of
// ThreadTypeLoggingLevelGreaterOrEqual() depends on it.
enum class LoggingLevel { kNone = 0, kUmaOnly = 1, kUmaAndCrash = 2 };

HangWatcher* g_instance = nullptr;
std::atomic<bool> g_use_hang_watcher{false};
std::atomic<LoggingLevel> g_threadpool_log_level{LoggingLevel::kNone};
std::atomic<LoggingLevel> g_io_thread_log_level{LoggingLevel::kNone};
std::atomic<LoggingLevel> g_ui_thread_log_level{LoggingLevel::kNone};

// Emits the hung thread count histogram. |count| is the number of threads
// of type |thread_type| that were hung or became hung during the last
// monitoring window. This function should be invoked for each thread type
// encountered on each call to Monitor().
void LogHungThreadCountHistogram(HangWatcher::ThreadType thread_type,
                                 int count) {
  // In the case of unique threads like the IO or UI thread a count does
  // not make sense.
  const bool any_thread_hung = count >= 1;

  switch (thread_type) {
    case HangWatcher::ThreadType::kIOThread:
      UMA_HISTOGRAM_BOOLEAN(
          "HangWatcher.IsThreadHung.BrowserProcess."
          "IOThread",
          any_thread_hung);
      break;
    case HangWatcher::ThreadType::kUIThread:
      UMA_HISTOGRAM_BOOLEAN(
          "HangWatcher.IsThreadHung.BrowserProcess."
          "UIThread",
          any_thread_hung);
      break;
    case HangWatcher::ThreadType::kThreadPoolThread:
      // Not recorded for now.
      break;
  }
}

// Returns true if |thread_type| was configured through Finch to have a logging
// level that is equal to or exceeds |logging_level|.
bool ThreadTypeLoggingLevelGreaterOrEqual(HangWatcher::ThreadType thread_type,
                                          LoggingLevel logging_level) {
  switch (thread_type) {
    case HangWatcher::ThreadType::kIOThread:
      return g_io_thread_log_level.load(std::memory_order_relaxed) >=
             logging_level;
    case HangWatcher::ThreadType::kUIThread:
      return g_ui_thread_log_level.load(std::memory_order_relaxed) >=
             logging_level;
    case HangWatcher::ThreadType::kThreadPoolThread:
      return g_threadpool_log_level.load(std::memory_order_relaxed) >=
             logging_level;
  }
}

}  // namespace

// Determines if the HangWatcher is activated. When false the HangWatcher
// thread never started.
const Feature kEnableHangWatcher{"EnableHangWatcher",
                                 FEATURE_ENABLED_BY_DEFAULT};

constexpr base::FeatureParam<int> kIOThreadLogLevel{
    &kEnableHangWatcher, "io_thread_log_level",
    static_cast<int>(LoggingLevel::kUmaOnly)};
constexpr base::FeatureParam<int> kUIThreadLogLevel{
    &kEnableHangWatcher, "ui_thread_log_level",
    static_cast<int>(LoggingLevel::kUmaOnly)};
constexpr base::FeatureParam<int> kThreadPoolLogLevel{
    &kEnableHangWatcher, "threadpool_log_level",
    static_cast<int>(LoggingLevel::kUmaOnly)};

// static
const base::TimeDelta WatchHangsInScope::kDefaultHangWatchTime =
    base::TimeDelta::FromSeconds(10);

constexpr const char* kThreadName = "HangWatcher";

// The time that the HangWatcher thread will sleep for between calls to
// Monitor(). Increasing or decreasing this does not modify the type of hangs
// that can be detected. It instead increases the probability that a call to
// Monitor() will happen at the right time to catch a hang. This has to be
// balanced with power/cpu use concerns as busy looping would catch amost all
// hangs but present unacceptable overhead. NOTE: If this period is ever changed
// then all metrics that depend on it like
// HangWatcher.IsThreadHung need to be updated.
constexpr auto kMonitoringPeriod = base::TimeDelta::FromSeconds(10);

WatchHangsInScope::WatchHangsInScope(TimeDelta timeout) {
  internal::HangWatchState* current_hang_watch_state =
      internal::HangWatchState::GetHangWatchStateForCurrentThread()->Get();

  DCHECK(timeout >= base::TimeDelta()) << "Negative timeouts are invalid.";

  // Thread is not monitored, noop.
  if (!current_hang_watch_state) {
    took_effect_ = false;
    return;
  }

#if DCHECK_IS_ON()
  previous_watch_hangs_in_scope_ =
      current_hang_watch_state->GetCurrentWatchHangsInScope();
  current_hang_watch_state->SetCurrentWatchHangsInScope(this);
#endif

  uint64_t old_flags;
  base::TimeTicks old_deadline;
  std::tie(old_flags, old_deadline) =
      current_hang_watch_state->GetFlagsAndDeadline();

  // TODO(crbug.com/1034046): Check whether we are over deadline already for the
  // previous WatchHangsInScope here by issuing only one TimeTicks::Now()
  // and resuing the value.

  previous_deadline_ = old_deadline;
  TimeTicks deadline = TimeTicks::Now() + timeout;
  current_hang_watch_state->SetDeadline(deadline);
  current_hang_watch_state->IncrementNestingLevel();

  const bool hangs_ignored_for_current_scope =
      internal::HangWatchDeadline::IsFlagSet(
          internal::HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope,
          old_flags);

  // If the current WatchHangsInScope is ignored, temporarily reactivate hang
  // watching for newly created WatchHangsInScopes. On exiting hang watching
  // is suspended again to return to the original state.
  if (hangs_ignored_for_current_scope) {
    current_hang_watch_state->UnsetIgnoreCurrentWatchHangsInScope();
    set_hangs_ignored_on_exit_ = true;
  }
}

WatchHangsInScope::~WatchHangsInScope() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  internal::HangWatchState* current_hang_watch_state =
      internal::HangWatchState::GetHangWatchStateForCurrentThread()->Get();

  // If hang watching was not enabled at construction time there is nothing to
  // validate or undo.
  if (!took_effect_) {
    return;
  }

  // If the thread was unregistered since construction there is also nothing to
  // do .
  if (!current_hang_watch_state) {
    return;
  }

  // If a hang is currently being captured we should block here so execution
  // stops and we avoid recording unrelated stack frames in the crash.
  if (current_hang_watch_state->IsFlagSet(
          internal::HangWatchDeadline::Flag::kShouldBlockOnHang))
    base::HangWatcher::GetInstance()->BlockIfCaptureInProgress();

#if DCHECK_IS_ON()
  // Verify that no Scope was destructed out of order.
  DCHECK_EQ(this, current_hang_watch_state->GetCurrentWatchHangsInScope());
  current_hang_watch_state->SetCurrentWatchHangsInScope(
      previous_watch_hangs_in_scope_);
#endif

  if (current_hang_watch_state->nesting_level() == 1) {
    // If a call to InvalidateActiveExpectations() suspended hang watching
    // during the lifetime of this or any nested WatchHangsInScope it can now
    // safely be reactivated by clearing the ignore bit since this is the
    // outer-most scope.
    current_hang_watch_state->UnsetIgnoreCurrentWatchHangsInScope();
  } else if (set_hangs_ignored_on_exit_) {
    // Return to ignoring hangs since this was the previous state before hang
    // watching was temporarily enabled for this WatchHangsInScope only in the
    // constructor.
    current_hang_watch_state->SetIgnoreCurrentWatchHangsInScope();
  }

  // Reset the deadline to the value it had before entering this
  // WatchHangsInScope.
  current_hang_watch_state->SetDeadline(previous_deadline_);
  // TODO(crbug.com/1034046): Log when a WatchHangsInScope exits after its
  // deadline and that went undetected by the HangWatcher.

  current_hang_watch_state->DecrementNestingLevel();
}

// static
void HangWatcher::InitializeOnMainThread() {
  DCHECK(!g_use_hang_watcher);
  DCHECK(g_io_thread_log_level == LoggingLevel::kNone);
  DCHECK(g_ui_thread_log_level == LoggingLevel::kNone);
  DCHECK(g_threadpool_log_level == LoggingLevel::kNone);

  g_use_hang_watcher.store(base::FeatureList::IsEnabled(kEnableHangWatcher),
                           std::memory_order_relaxed);

  // If hang watching is disabled as a whole there is no need to read the
  // params.
  if (g_use_hang_watcher.load(std::memory_order_relaxed)) {
    g_threadpool_log_level.store(
        static_cast<LoggingLevel>(kThreadPoolLogLevel.Get()),
        std::memory_order_relaxed);
    g_io_thread_log_level.store(
        static_cast<LoggingLevel>(kIOThreadLogLevel.Get()),
        std::memory_order_relaxed);
    g_ui_thread_log_level.store(
        static_cast<LoggingLevel>(kUIThreadLogLevel.Get()),
        std::memory_order_relaxed);
  }
}

void HangWatcher::UnitializeOnMainThreadForTesting() {
  g_use_hang_watcher.store(false, std::memory_order_relaxed);
  g_threadpool_log_level.store(LoggingLevel::kNone, std::memory_order_relaxed);
  g_io_thread_log_level.store(LoggingLevel::kNone, std::memory_order_relaxed);
  g_ui_thread_log_level.store(LoggingLevel::kNone, std::memory_order_relaxed);
}

// static
bool HangWatcher::IsEnabled() {
  return g_use_hang_watcher.load(std::memory_order_relaxed);
}

// static
bool HangWatcher::IsThreadPoolHangWatchingEnabled() {
  return g_threadpool_log_level.load(std::memory_order_relaxed) !=
         LoggingLevel::kNone;
}

// static
bool HangWatcher::IsIOThreadHangWatchingEnabled() {
  return g_io_thread_log_level.load(std::memory_order_relaxed) !=
         LoggingLevel::kNone;
}

// static
bool HangWatcher::IsCrashReportingEnabled() {
  if (g_ui_thread_log_level.load(std::memory_order_relaxed) ==
      LoggingLevel::kUmaAndCrash) {
    return true;
  }
  if (g_io_thread_log_level.load(std::memory_order_relaxed) ==
      LoggingLevel::kUmaAndCrash) {
    return true;
  }
  if (g_threadpool_log_level.load(std::memory_order_relaxed) ==
      LoggingLevel::kUmaAndCrash) {
    return true;
  }
  return false;
}

// static
void HangWatcher::InvalidateActiveExpectations() {
  internal::HangWatchState* current_hang_watch_state =
      internal::HangWatchState::GetHangWatchStateForCurrentThread()->Get();
  if (!current_hang_watch_state) {
    // If the current thread is not under watch there is nothing to invalidate.
    return;
  }
  current_hang_watch_state->SetIgnoreCurrentWatchHangsInScope();
}

HangWatcher::HangWatcher()
    : monitor_period_(kMonitoringPeriod),
      should_monitor_(WaitableEvent::ResetPolicy::AUTOMATIC),
      thread_(this, kThreadName),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      memory_pressure_listener_(
          FROM_HERE,
          base::BindRepeating(&HangWatcher::OnMemoryPressure,
                              base::Unretained(this))) {
  // |thread_checker_| should not be bound to the constructing thread.
  DETACH_FROM_THREAD(hang_watcher_thread_checker_);

  should_monitor_.declare_only_used_while_idle();

  DCHECK(!g_instance);
  g_instance = this;
}

#if not defined(OS_NACL)
debug::ScopedCrashKeyString
HangWatcher::GetTimeSinceLastCriticalMemoryPressureCrashKey() {
  DCHECK_CALLED_ON_VALID_THREAD(hang_watcher_thread_checker_);

  // The crash key size is large enough to hold the biggest possible return
  // value from base::TimeDelta::InSeconds().
  constexpr debug::CrashKeySize kCrashKeyContentSize =
      debug::CrashKeySize::Size32;
  DCHECK_GE(static_cast<uint64_t>(kCrashKeyContentSize),
            base::NumberToString(std::numeric_limits<int64_t>::max()).size());

  static debug::CrashKeyString* crash_key = AllocateCrashKeyString(
      "seconds-since-last-memory-pressure", kCrashKeyContentSize);

  const base::TimeTicks last_critical_memory_pressure_time =
      last_critical_memory_pressure_.load(std::memory_order_relaxed);
  if (last_critical_memory_pressure_time.is_null()) {
    constexpr char kNoMemoryPressureMsg[] = "No critical memory pressure";
    static_assert(
        base::size(kNoMemoryPressureMsg) <=
            static_cast<uint64_t>(kCrashKeyContentSize),
        "The crash key is too small to hold \"No critical memory pressure\".");
    return debug::ScopedCrashKeyString(crash_key, kNoMemoryPressureMsg);
  } else {
    base::TimeDelta time_since_last_critical_memory_pressure =
        base::TimeTicks::Now() - last_critical_memory_pressure_time;
    return debug::ScopedCrashKeyString(
        crash_key, base::NumberToString(
                       time_since_last_critical_memory_pressure.InSeconds()));
  }
}
#endif

void HangWatcher::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    last_critical_memory_pressure_.store(base::TimeTicks::Now(),
                                         std::memory_order_relaxed);
  }
}

HangWatcher::~HangWatcher() {
  DCHECK_EQ(g_instance, this);
  DCHECK(watch_states_.empty());
  g_instance = nullptr;
  Stop();
}

void HangWatcher::Start() {
  thread_.Start();
}

void HangWatcher::Stop() {
  keep_monitoring_.store(false, std::memory_order_relaxed);
  should_monitor_.Signal();
  thread_.Join();
}

bool HangWatcher::IsWatchListEmpty() {
  AutoLock auto_lock(watch_state_lock_);
  return watch_states_.empty();
}

void HangWatcher::Wait() {
  while (true) {
    // Amount by which the actual time spent sleeping can deviate from
    // the target time and still be considered timely.
    constexpr base::TimeDelta kWaitDriftTolerance =
        base::TimeDelta::FromMilliseconds(100);

    const base::TimeTicks time_before_wait = tick_clock_->NowTicks();

    // Sleep until next scheduled monitoring or until signaled.
    const bool was_signaled = should_monitor_.TimedWait(monitor_period_);

    if (after_wait_callback_)
      after_wait_callback_.Run(time_before_wait);

    const base::TimeTicks time_after_wait = tick_clock_->NowTicks();
    const base::TimeDelta wait_time = time_after_wait - time_before_wait;
    const bool wait_was_normal =
        wait_time <= (monitor_period_ + kWaitDriftTolerance);

    UMA_HISTOGRAM_TIMES("HangWatcher.SleepDrift.BrowserProcess",
                        wait_time - monitor_period_);

    if (!wait_was_normal) {
      // If the time spent waiting was too high it might indicate the machine is
      // very slow or that that it went to sleep. In any case we can't trust the
      // WatchHangsInScopes that are currently live. Update the ignore
      // threshold to make sure they don't trigger a hang on subsequent monitors
      // then keep waiting.

      base::AutoLock auto_lock(watch_state_lock_);

      // Find the latest deadline among the live watch states. They might change
      // atomically while iterating but that's fine because if they do that
      // means the new WatchHangsInScope was constructed very soon after the
      // abnormal sleep happened and might be affected by the root cause still.
      // Ignoring it is cautious and harmless.
      base::TimeTicks latest_deadline;
      for (const auto& state : watch_states_) {
        base::TimeTicks deadline = state->GetDeadline();
        if (deadline > latest_deadline) {
          latest_deadline = deadline;
        }
      }

      deadline_ignore_threshold_ = latest_deadline;
    }

    // Stop waiting.
    if (wait_was_normal || was_signaled)
      return;
  }
}

void HangWatcher::Run() {
  // Monitor() should only run on |thread_|. Bind |thread_checker_| here to make
  // sure of that.
  DCHECK_CALLED_ON_VALID_THREAD(hang_watcher_thread_checker_);

  while (keep_monitoring_.load(std::memory_order_relaxed)) {
    Wait();

    if (!IsWatchListEmpty() &&
        keep_monitoring_.load(std::memory_order_relaxed)) {
      Monitor();
      if (after_monitor_closure_for_testing_) {
        after_monitor_closure_for_testing_.Run();
      }
    }
  }
}

// static
HangWatcher* HangWatcher::GetInstance() {
  return g_instance;
}

// static
void HangWatcher::RecordHang() {
  base::debug::DumpWithoutCrashing();
  NO_CODE_FOLDING();
}

ScopedClosureRunner HangWatcher::RegisterThreadInternal(
    ThreadType thread_type) {
  AutoLock auto_lock(watch_state_lock_);

  watch_states_.push_back(
      internal::HangWatchState::CreateHangWatchStateForCurrentThread(
          thread_type));

  return ScopedClosureRunner(BindOnce(&HangWatcher::UnregisterThread,
                                      Unretained(HangWatcher::GetInstance())));
}

// static
ScopedClosureRunner HangWatcher::RegisterThread(ThreadType thread_type) {
  if (!GetInstance()) {
    return ScopedClosureRunner();
  }

  return GetInstance()->RegisterThreadInternal(thread_type);
}

base::TimeTicks HangWatcher::WatchStateSnapShot::GetHighestDeadline() const {
  DCHECK(!hung_watch_state_copies_.empty());
  // Since entries are sorted in increasing order the last entry is the largest
  // one.
  return hung_watch_state_copies_.back().deadline;
}

HangWatcher::WatchStateSnapShot::WatchStateSnapShot(
    const HangWatchStates& watch_states,
    base::TimeTicks deadline_ignore_threshold) {
  const base::TimeTicks now = base::TimeTicks::Now();
  bool all_threads_marked = true;
  bool found_deadline_before_ignore_threshold = false;

  // Use an std::array to store the hang counts to avoid allocations. The
  // numerical values of the HangWatcher::ThreadType enum is used to index into
  // the array. A |kInvalidHangCount| is used to signify there were no threads
  // of the type found.
  constexpr size_t kHangCountArraySize =
      static_cast<std::size_t>(base::HangWatcher::ThreadType::kMax) + 1;
  std::array<int, kHangCountArraySize> hung_counts_per_thread_type;

  constexpr int kInvalidHangCount = -1;
  hung_counts_per_thread_type.fill(kInvalidHangCount);

  // Will be true if any of the hung threads has a logging level high enough,
  // as defined through finch params, to warant dumping a crash.
  bool any_hung_thread_has_dumping_enabled = false;

  // Copy hung thread information.
  for (const auto& watch_state : watch_states) {
    uint64_t flags;
    base::TimeTicks deadline;
    std::tie(flags, deadline) = watch_state->GetFlagsAndDeadline();

    if (deadline <= deadline_ignore_threshold) {
      found_deadline_before_ignore_threshold = true;
    }

    if (internal::HangWatchDeadline::IsFlagSet(
            internal::HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope,
            flags)) {
      continue;
    }

    // If a thread type is monitored and did not hang it still needs to be
    // logged as a zero count;
    const size_t hang_count_index =
        static_cast<size_t>(watch_state.get()->thread_type());
    if (hung_counts_per_thread_type[hang_count_index] == kInvalidHangCount) {
      hung_counts_per_thread_type[hang_count_index] = 0;
    }

    // Only copy hung threads.
    if (deadline <= now) {
      ++hung_counts_per_thread_type[hang_count_index];

      if (ThreadTypeLoggingLevelGreaterOrEqual(watch_state.get()->thread_type(),
                                               LoggingLevel::kUmaAndCrash)) {
        any_hung_thread_has_dumping_enabled = true;
      }

      // Attempt to mark the thread as needing to stay within its current
      // WatchHangsInScope until capture is complete.
      bool thread_marked = watch_state->SetShouldBlockOnHang(flags, deadline);

      // If marking some threads already failed the snapshot won't be kept so
      // there is no need to keep adding to it. The loop doesn't abort though
      // to keep marking the other threads. If these threads remain hung until
      // the next capture then they'll already be marked and will be included
      // in the capture at that time.
      if (thread_marked && all_threads_marked) {
        hung_watch_state_copies_.push_back(WatchStateCopy{
            deadline,
            static_cast<PlatformThreadId>(watch_state.get()->GetThreadID())});
      } else {
        all_threads_marked = false;
      }
    }
  }

  // Log the hung thread counts to histograms for each thread type if any thread
  // of the type were found.
  for (size_t i = 0; i < kHangCountArraySize; ++i) {
    const int hang_count = hung_counts_per_thread_type[i];
    const HangWatcher::ThreadType thread_type =
        static_cast<HangWatcher::ThreadType>(i);
    if (hang_count != kInvalidHangCount &&
        ThreadTypeLoggingLevelGreaterOrEqual(thread_type,
                                             LoggingLevel::kUmaOnly))
      LogHungThreadCountHistogram(thread_type, hang_count);
  }

  // Three cases can invalidate this snapshot and prevent the capture of the
  // hang.
  //
  // 1. Some threads could not be marked for blocking so this snapshot isn't
  // actionable since marked threads could be hung because of unmarked ones.
  // If only the marked threads were captured the information would be
  // incomplete.
  //
  // 2. Any of the threads have a deadline before |deadline_ignore_threshold|.
  // If any thread is ignored it reduces the confidence in the whole state and
  // it's better to avoid capturing misleading data.
  //
  // 3. The hung threads found were all of types that are not configured through
  // Finch to trigger a crash dump.
  //
  if (!all_threads_marked || found_deadline_before_ignore_threshold ||
      !any_hung_thread_has_dumping_enabled) {
    hung_watch_state_copies_.clear();
    return;
  }

  // Sort |hung_watch_state_copies_| by order of decreasing hang severity so the
  // most severe hang is first in the list.
  ranges::sort(hung_watch_state_copies_,
               [](const WatchStateCopy& lhs, const WatchStateCopy& rhs) {
                 return lhs.deadline < rhs.deadline;
               });
}

HangWatcher::WatchStateSnapShot::WatchStateSnapShot(
    const WatchStateSnapShot& other) = default;

HangWatcher::WatchStateSnapShot::~WatchStateSnapShot() = default;

std::string HangWatcher::WatchStateSnapShot::PrepareHungThreadListCrashKey()
    const {
  DCHECK(IsActionable());

  // Build a crash key string that contains the ids of the hung threads.
  constexpr char kSeparator{'|'};
  std::string list_of_hung_thread_ids;

  // Add as many thread ids to the crash key as possible.
  for (const WatchStateCopy& copy : hung_watch_state_copies_) {
    std::string fragment = base::NumberToString(copy.thread_id) + kSeparator;
    if (list_of_hung_thread_ids.size() + fragment.size() <
        static_cast<std::size_t>(debug::CrashKeySize::Size256)) {
      list_of_hung_thread_ids += fragment;
    } else {
      // Respect the by priority ordering of thread ids in the crash key by
      // stopping the construction as soon as one does not fit. This avoids
      // including lesser priority ids while omitting more important ones.
      break;
    }
  }

  return list_of_hung_thread_ids;
}

bool HangWatcher::WatchStateSnapShot::IsActionable() const {
  return !hung_watch_state_copies_.empty();
}

HangWatcher::WatchStateSnapShot HangWatcher::GrabWatchStateSnapshotForTesting()
    const {
  WatchStateSnapShot snapshot(watch_states_, deadline_ignore_threshold_);
  return snapshot;
}

void HangWatcher::Monitor() {
  DCHECK_CALLED_ON_VALID_THREAD(hang_watcher_thread_checker_);
  AutoLock auto_lock(watch_state_lock_);

  // If all threads unregistered since this function was invoked there's
  // nothing to do anymore.
  if (watch_states_.empty())
    return;

  WatchStateSnapShot watch_state_snapshot(watch_states_,
                                          deadline_ignore_threshold_);

  if (watch_state_snapshot.IsActionable()) {
    DoDumpWithoutCrashing(watch_state_snapshot);
  }
}

void HangWatcher::DoDumpWithoutCrashing(
    const WatchStateSnapShot& watch_state_snapshot) {
  capture_in_progress_.store(true, std::memory_order_relaxed);
  base::AutoLock scope_lock(capture_lock_);

#if not defined(OS_NACL)
  const std::string list_of_hung_thread_ids =
      watch_state_snapshot.PrepareHungThreadListCrashKey();

  static debug::CrashKeyString* crash_key = AllocateCrashKeyString(
      "list-of-hung-threads", debug::CrashKeySize::Size256);

  const debug::ScopedCrashKeyString list_of_hung_threads_crash_key_string(
      crash_key, list_of_hung_thread_ids);

  const debug::ScopedCrashKeyString
      time_since_last_critical_memory_pressure_crash_key_string =
          GetTimeSinceLastCriticalMemoryPressureCrashKey();
#endif

  // To avoid capturing more than one hang that blames a subset of the same
  // threads it's necessary to keep track of what is the furthest deadline
  // that contributed to declaring a hang. Only once
  // all threads have deadlines past this point can we be sure that a newly
  // discovered hang is not directly related.
  // Example:
  // **********************************************************************
  // Timeline A : L------1-------2----------3-------4----------N-----------
  // Timeline B : -------2----------3-------4----------L----5------N-------
  // Timeline C : L----------------------------5------6----7---8------9---N
  // **********************************************************************
  // In the example when a Monitor() happens during timeline A
  // |deadline_ignore_threshold_| (L) is at time zero and deadlines (1-4)
  // are before Now() (N) . A hang is captured and L is updated. During
  // the next Monitor() (timeline B) a new deadline is over but we can't
  // capture a hang because deadlines 2-4 are still live and already counted
  // toward a hang. During a third monitor (timeline C) all live deadlines
  // are now after L and a second hang can be recorded.
  base::TimeTicks latest_expired_deadline =
      watch_state_snapshot.GetHighestDeadline();

  if (on_hang_closure_for_testing_)
    on_hang_closure_for_testing_.Run();
  else
    RecordHang();

  // Update after running the actual capture.
  deadline_ignore_threshold_ = latest_expired_deadline;

  capture_in_progress_.store(false, std::memory_order_relaxed);
}

void HangWatcher::SetAfterMonitorClosureForTesting(
    base::RepeatingClosure closure) {
  DCHECK_CALLED_ON_VALID_THREAD(constructing_thread_checker_);
  after_monitor_closure_for_testing_ = std::move(closure);
}

void HangWatcher::SetOnHangClosureForTesting(base::RepeatingClosure closure) {
  DCHECK_CALLED_ON_VALID_THREAD(constructing_thread_checker_);
  on_hang_closure_for_testing_ = std::move(closure);
}

void HangWatcher::SetMonitoringPeriodForTesting(base::TimeDelta period) {
  DCHECK_CALLED_ON_VALID_THREAD(constructing_thread_checker_);
  monitor_period_ = period;
}

void HangWatcher::SetAfterWaitCallbackForTesting(
    RepeatingCallback<void(TimeTicks)> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(constructing_thread_checker_);
  after_wait_callback_ = callback;
}

void HangWatcher::SignalMonitorEventForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(constructing_thread_checker_);
  should_monitor_.Signal();
}

void HangWatcher::StopMonitoringForTesting() {
  keep_monitoring_.store(false, std::memory_order_relaxed);
}

void HangWatcher::SetTickClockForTesting(const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void HangWatcher::BlockIfCaptureInProgress() {
  // Makes a best-effort attempt to block execution if a hang is currently being
  // captured.Only block on |capture_lock| if |capture_in_progress_| hints that
  // it's already held to avoid serializing all threads on this function when no
  // hang capture is in-progress.
  if (capture_in_progress_.load(std::memory_order_relaxed))
    base::AutoLock hang_lock(capture_lock_);
}

void HangWatcher::UnregisterThread() {
  AutoLock auto_lock(watch_state_lock_);

  internal::HangWatchState* current_hang_watch_state =
      internal::HangWatchState::GetHangWatchStateForCurrentThread()->Get();

  auto it = ranges::find_if(
      watch_states_,
      [current_hang_watch_state](
          const std::unique_ptr<internal::HangWatchState>& state) {
        return state.get() == current_hang_watch_state;
      });

  // Thread should be registered to get unregistered.
  DCHECK(it != watch_states_.end());

  watch_states_.erase(it);
}

namespace internal {
namespace {

constexpr uint64_t kOnlyDeadlineMask = 0x00FFFFFFFFFFFFFFu;
constexpr uint64_t kOnlyFlagsMask = ~kOnlyDeadlineMask;
constexpr uint64_t kMaximumFlag = 0x8000000000000000u;

// Use as a mask to keep persistent flags and the deadline.
constexpr uint64_t kPersistentFlagsAndDeadlineMask =
    kOnlyDeadlineMask |
    static_cast<uint64_t>(
        HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope);
}  // namespace

// Flag binary representation assertions.
static_assert(
    static_cast<uint64_t>(HangWatchDeadline::Flag::kMinValue) >
        kOnlyDeadlineMask,
    "Invalid numerical value for flag. Would interfere with bits of data.");
static_assert(static_cast<uint64_t>(HangWatchDeadline::Flag::kMaxValue) <=
                  kMaximumFlag,
              "A flag can only set a single bit.");

HangWatchDeadline::HangWatchDeadline() = default;
HangWatchDeadline::~HangWatchDeadline() = default;

std::pair<uint64_t, TimeTicks> HangWatchDeadline::GetFlagsAndDeadline() const {
  uint64_t bits = bits_.load(std::memory_order_relaxed);
  return std::make_pair(ExtractFlags(bits),
                        DeadlineFromBits(ExtractDeadline((bits))));
}

TimeTicks HangWatchDeadline::GetDeadline() const {
  return DeadlineFromBits(
      ExtractDeadline(bits_.load(std::memory_order_relaxed)));
}

// static
TimeTicks HangWatchDeadline::Max() {
  // |kOnlyDeadlineMask| has all the bits reserved for the TimeTicks value
  // set. This means it also represents the highest representable value.
  return DeadlineFromBits(kOnlyDeadlineMask);
}

// static
bool HangWatchDeadline::IsFlagSet(Flag flag, uint64_t flags) {
  return static_cast<uint64_t>(flag) & flags;
}

void HangWatchDeadline::SetDeadline(TimeTicks new_deadline) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(new_deadline <= Max()) << "Value too high to be represented.";
  DCHECK(new_deadline >= TimeTicks{}) << "Value cannot be negative.";

  if (switch_bits_callback_for_testing_) {
    const uint64_t switched_in_bits = SwitchBitsForTesting();
    // If a concurrent deadline change is tested it cannot have a deadline or
    // persistent flag change since those always happen on the same thread.
    DCHECK((switched_in_bits & kPersistentFlagsAndDeadlineMask) == 0u);
  }

  // Discard all non-persistent flags and apply deadline change.
  const uint64_t old_bits = bits_.load(std::memory_order_relaxed);
  const uint64_t new_flags =
      ExtractFlags(old_bits & kPersistentFlagsAndDeadlineMask);
  bits_.store(new_flags | ExtractDeadline(new_deadline.ToInternalValue()),
              std::memory_order_relaxed);
}

// TODO(crbug.com/1087026): Add flag DCHECKs here.
bool HangWatchDeadline::SetShouldBlockOnHang(uint64_t old_flags,
                                             TimeTicks old_deadline) {
  DCHECK(old_deadline <= Max()) << "Value too high to be represented.";
  DCHECK(old_deadline >= TimeTicks{}) << "Value cannot be negative.";

  // Set the kShouldBlockOnHang flag only if |bits_| did not change since it was
  // read. kShouldBlockOnHang is the only non-persistent flag and should never
  // be set twice. Persistent flags and deadline changes are done from the same
  // thread so there is no risk of losing concurrently added information.
  uint64_t old_bits =
      old_flags | static_cast<uint64_t>(old_deadline.ToInternalValue());
  const uint64_t desired_bits =
      old_bits | static_cast<uint64_t>(Flag::kShouldBlockOnHang);

  // If a test needs to simulate |bits_| changing since calling this function
  // this happens now.
  if (switch_bits_callback_for_testing_) {
    const uint64_t switched_in_bits = SwitchBitsForTesting();

    // Injecting the flag being tested is invalid.
    DCHECK(!IsFlagSet(Flag::kShouldBlockOnHang, switched_in_bits));
  }

  return bits_.compare_exchange_weak(old_bits, desired_bits,
                                     std::memory_order_relaxed,
                                     std::memory_order_relaxed);
}

void HangWatchDeadline::SetIgnoreCurrentWatchHangsInScope() {
  SetPersistentFlag(Flag::kIgnoreCurrentWatchHangsInScope);
}

void HangWatchDeadline::UnsetIgnoreCurrentWatchHangsInScope() {
  ClearPersistentFlag(Flag::kIgnoreCurrentWatchHangsInScope);
}

void HangWatchDeadline::SetPersistentFlag(Flag flag) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (switch_bits_callback_for_testing_)
    SwitchBitsForTesting();
  bits_.fetch_or(static_cast<uint64_t>(flag), std::memory_order_relaxed);
}

void HangWatchDeadline::ClearPersistentFlag(Flag flag) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (switch_bits_callback_for_testing_)
    SwitchBitsForTesting();
  bits_.fetch_and(~(static_cast<uint64_t>(flag)), std::memory_order_relaxed);
}

// static
uint64_t HangWatchDeadline::ExtractFlags(uint64_t bits) {
  return bits & kOnlyFlagsMask;
}

// static
uint64_t HangWatchDeadline::ExtractDeadline(uint64_t bits) {
  return bits & kOnlyDeadlineMask;
}

// static
TimeTicks HangWatchDeadline::DeadlineFromBits(uint64_t bits) {
  // |kOnlyDeadlineMask| has all the deadline bits set to 1 so is the largest
  // representable value.
  DCHECK(bits <= kOnlyDeadlineMask)
      << "Flags bits are set. Remove them before returning deadline.";
  return TimeTicks::FromInternalValue(bits);
}

bool HangWatchDeadline::IsFlagSet(Flag flag) const {
  return bits_.load(std::memory_order_relaxed) & static_cast<uint64_t>(flag);
}

void HangWatchDeadline::SetSwitchBitsClosureForTesting(
    RepeatingCallback<uint64_t(void)> closure) {
  switch_bits_callback_for_testing_ = closure;
}

void HangWatchDeadline::ResetSwitchBitsClosureForTesting() {
  DCHECK(switch_bits_callback_for_testing_);
  switch_bits_callback_for_testing_.Reset();
}

uint64_t HangWatchDeadline::SwitchBitsForTesting() {
  DCHECK(switch_bits_callback_for_testing_);

  const uint64_t old_bits = bits_.load(std::memory_order_relaxed);
  const uint64_t new_bits = switch_bits_callback_for_testing_.Run();
  const uint64_t old_flags = ExtractFlags(old_bits);

  const uint64_t switched_in_bits = old_flags | new_bits;
  bits_.store(switched_in_bits, std::memory_order_relaxed);
  return switched_in_bits;
}

HangWatchState::HangWatchState(HangWatcher::ThreadType thread_type)
    : thread_type_(thread_type) {
  // There should not exist a state object for this thread already.
  DCHECK(!GetHangWatchStateForCurrentThread()->Get());

// TODO(crbug.com/1223033): Remove this once macOS uses system-wide ids.
// On macOS the thread ids used by CrashPad are not the same as the ones
// provided by PlatformThread. Make sure to use the same for correct
// attribution.
#ifdef OS_MAC
  pthread_threadid_np(pthread_self(), &thread_id_);
#else
  thread_id_ = PlatformThread::CurrentId();
#endif

  // Bind the new instance to this thread.
  GetHangWatchStateForCurrentThread()->Set(this);
}

HangWatchState::~HangWatchState() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK_EQ(GetHangWatchStateForCurrentThread()->Get(), this);
  GetHangWatchStateForCurrentThread()->Set(nullptr);

#if DCHECK_IS_ON()
  // Destroying the HangWatchState should not be done if there are live
  // WatchHangsInScopes.
  DCHECK(!current_watch_hangs_in_scope_);
#endif
}

// static
std::unique_ptr<HangWatchState>
HangWatchState::CreateHangWatchStateForCurrentThread(
    HangWatcher::ThreadType thread_type) {
  // Allocate a watch state object for this thread.
  std::unique_ptr<HangWatchState> hang_state =
      std::make_unique<HangWatchState>(thread_type);

  // Setting the thread local worked.
  DCHECK_EQ(GetHangWatchStateForCurrentThread()->Get(), hang_state.get());

  // Transfer ownership to caller.
  return hang_state;
}

TimeTicks HangWatchState::GetDeadline() const {
  return deadline_.GetDeadline();
}

std::pair<uint64_t, TimeTicks> HangWatchState::GetFlagsAndDeadline() const {
  return deadline_.GetFlagsAndDeadline();
}

void HangWatchState::SetDeadline(TimeTicks deadline) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  deadline_.SetDeadline(deadline);
}

bool HangWatchState::IsOverDeadline() const {
  return TimeTicks::Now() > deadline_.GetDeadline();
}

void HangWatchState::SetIgnoreCurrentWatchHangsInScope() {
  deadline_.SetIgnoreCurrentWatchHangsInScope();
}

void HangWatchState::UnsetIgnoreCurrentWatchHangsInScope() {
  deadline_.UnsetIgnoreCurrentWatchHangsInScope();
}

bool HangWatchState::SetShouldBlockOnHang(uint64_t old_flags,
                                          TimeTicks old_deadline) {
  return deadline_.SetShouldBlockOnHang(old_flags, old_deadline);
}

bool HangWatchState::IsFlagSet(HangWatchDeadline::Flag flag) {
  return deadline_.IsFlagSet(flag);
}

#if DCHECK_IS_ON()
void HangWatchState::SetCurrentWatchHangsInScope(
    WatchHangsInScope* current_hang_watch_scope_enable) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  current_watch_hangs_in_scope_ = current_hang_watch_scope_enable;
}

WatchHangsInScope* HangWatchState::GetCurrentWatchHangsInScope() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return current_watch_hangs_in_scope_;
}
#endif

HangWatchDeadline* HangWatchState::GetHangWatchDeadlineForTesting() {
  return &deadline_;
}

void HangWatchState::IncrementNestingLevel() {
  ++nesting_level_;
}

void HangWatchState::DecrementNestingLevel() {
  --nesting_level_;
}

// static
ThreadLocalPointer<HangWatchState>*
HangWatchState::GetHangWatchStateForCurrentThread() {
  static NoDestructor<ThreadLocalPointer<HangWatchState>> hang_watch_state;
  return hang_watch_state.get();
}

uint64_t HangWatchState::GetThreadID() const {
  return thread_id_;
}

}  // namespace internal

}  // namespace base
