// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_RESPONSIVENESS_WATCHER_H_
#define BASE_TASK_RESPONSIVENESS_WATCHER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/base_export.h"
#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/responsiveness/calculator.h"
#include "base/task/responsiveness/responsiveness_calculator_delegate.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"

namespace base {

struct PendingTask;

namespace responsiveness {

class MessageLoopObserver;

// Observes tasks and native OS events on the main thread (`SetUp()`,
// `WillRunEventOnMainThread()` / `DidRunEventOnMainThread()`) and tasks on
// secondary threads (`RegisterCurrentThread()`), and feeds `(queue_time,
// execution_start_time, execution_finish_time)` tuples to an owned `Calculator`
// (directly on the main thread, or via `SecondarySources` on secondary
// threads):
// - Ignores tasks/events that caused reentrancy and immediate tasks posted
//   before observation started.
// - Treats queueing time as zero (`queue_time = execution_start_time`) for
//   delayed tasks, blocked/low-priority tasks, and native events.
//
// Instance methods are main-thread affine; `RegisterCurrentThread()` runs on
// each monitored secondary thread.
class BASE_EXPORT Watcher {
 public:
  struct Config {
    // Optional breakdown histogram suffix for the main thread. When non-empty,
    // main-thread congestion is also emitted to
    // `<histogram_prefix>.MainThreadsCongestion.<main_source_name>*` (in
    // addition to the union histograms).
    std::string_view main_source_name = "";
    // Whether this Watcher's Calculator consumes congestions from
    // `SecondarySources::Get()` (which must be non-null when true).
    // Defaults to false so standalone single-thread Watchers do not collide
    // with a process-wide Watcher.
    bool consume_secondary_sources = false;
    // If true, `SetUp()` schedules `OnFirstIdle()` on next main-thread idle to
    // end startup and begin recording queuing congestion. Set to false when the
    // owner calls `OnFirstIdle()` manually (e.g. `BrowserMainLoop`, to order it
    // after `BrowserMainParts::OnFirstIdle()`).
    bool register_first_idle_callback = true;
    // Whether to observe `ApplicationStatusListener` on Android to treat
    // backgrounded browser application states as suspended.
    bool observe_application_status = false;
    // Optional delegate notified of measurement intervals and emitted samples.
    std::unique_ptr<ResponsivenessCalculatorDelegate> delegate;
  };

  explicit Watcher(std::string_view histogram_prefix);
  Watcher(std::string_view histogram_prefix, Config config);

  Watcher(const Watcher&) = delete;
  Watcher& operator=(const Watcher&) = delete;

  virtual ~Watcher();

  // Creates the `Calculator`, starts observing tasks on the calling (main)
  // thread, attaches to `SecondarySources` if `consume_secondary_sources` is
  // true, and schedules `OnFirstIdle()` if `register_first_idle_callback` is
  // true. Separated from the constructor so subclasses can override
  // `CreateCalculator()` / `ShouldRegisterMessageLoopObserver()`.
  virtual void SetUp();

  // Creates the process-wide `SecondarySources` registry. Must be called before
  // secondary threads start: `RegisterCurrentThread()` is a no-op when
  // `SecondarySources::Get()` is null (avoiding `MessageLoopObserver` overhead
  // in unmonitored processes whose threads call `RegisterCurrentThread()`).
  static void EnableForProcess();

  // Registers the calling thread as a secondary thread in
  // `SecondarySources::Get()` (emitting optional breakdown histograms under
  // `source_name` when non-empty). Must only be called on process-lifetime
  // singleton threads (registered sources remain in `SecondarySources` for the
  // lifetime of the registry, and non-empty `source_name`s must be unique).
  // Returns an empty runner if no `SecondarySources` exists. Destroy the
  // returned `ScopedClosureRunner` on
  // the same thread (e.g. in `CleanUp()` or when `Run()` returns) while
  // `CurrentThread` is still valid and before the `SecondarySources` is
  // destroyed.
  [[nodiscard]] static ScopedClosureRunner RegisterCurrentThread(
      std::string_view source_name = "");

  // Signals the end of startup on the main thread. Called automatically from
  // SetUp()'s idle callback when `register_first_idle_callback` is true;
  // otherwise the owner must call it once-and-only-once, after SetUp(), the
  // first time the main thread reaches idle.
  void OnFirstIdle();

 protected:
  // Exposed for tests.
  virtual std::unique_ptr<Calculator> CreateCalculator();
  virtual bool ShouldRegisterMessageLoopObserver() const;
  bool consume_secondary_sources() const { return consume_secondary_sources_; }

  void WillRunEventOnMainThread(uintptr_t opaque_identifier);
  void DidRunEventOnMainThread(uintptr_t opaque_identifier);

 private:
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, TaskForwarding);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, TaskNesting);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, NativeEvents);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, BlockedOrLowPriorityTask);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, DelayedTask);

  // Metadata for currently running tasks and events is needed to track whether
  // or not they caused reentrancy.
  struct Metadata {
    explicit Metadata(uintptr_t identifier,
                      bool was_blocked_or_low_priority,
                      base::TimeTicks execution_start_time);

    // An opaque identifier for the task or event.
    //
    // `identifier` is not a raw_ptr<...> for performance reasons (based on
    // analysis of sampling profiler data and tab_search:top100:2020).
    uintptr_t const identifier;

    // Whether the task was at some point in a queue that was blocked or low
    // priority.
    const bool was_blocked_or_low_priority;

    // The time at which the task or event started running.
    const base::TimeTicks execution_start_time;

    // Whether the task or event has caused reentrancy.
    bool caused_reentrancy = false;
  };

  // Per-thread task/event observer affine to the sequence on which it is
  // constructed. Main-thread instances forward directly to `calculator_`;
  // secondary-thread instances forward directly to `secondary_sources_`.
  class BASE_EXPORT ThreadObserver {
   public:
    ThreadObserver(Calculator* calculator, bool register_message_loop_observer);
    ThreadObserver(SecondarySources* secondary_sources,
                   SecondarySources::SourceId secondary_source_id,
                   bool register_message_loop_observer);
    ThreadObserver(const ThreadObserver&) = delete;
    ThreadObserver& operator=(const ThreadObserver&) = delete;
    ~ThreadObserver();

    void WillRunTask(const base::PendingTask* task,
                     bool was_blocked_or_low_priority);
    void DidRunTask(const base::PendingTask* task);
    void WillRunEvent(uintptr_t opaque_identifier);
    void DidRunEvent(uintptr_t opaque_identifier);

   private:
    ThreadObserver(Calculator* calculator,
                   SecondarySources* secondary_sources,
                   SecondarySources::SourceId secondary_source_id,
                   bool register_message_loop_observer);

    void RecordTaskOrEventFinished(base::TimeTicks queue_time,
                                   base::TimeTicks execution_start_time,
                                   base::TimeTicks execution_finish_time);

    // Exactly one of `calculator_` (main-thread observer; outlives `this`
    // because `~Watcher()` resets `main_thread_observer_` first) or
    // `secondary_sources_` (secondary-thread observer; outlives all registered
    // `ThreadObserver`s) is non-null, receiving completed task/event timestamps
    // via `RecordTaskOrEventFinished()`.
    const raw_ptr<Calculator> calculator_ = nullptr;
    const raw_ptr<SecondarySources> secondary_sources_ = nullptr;
    const SecondarySources::SourceId secondary_source_id_;

    SEQUENCE_CHECKER(sequence_checker_);

    std::vector<Metadata> currently_running_metadata_
        GUARDED_BY_CONTEXT(sequence_checker_);

    // Task/event identifiers should be mismatched at most once, since the
    // observer may register itself during a task or event execution and thus
    // miss the initial WillRun* callback.
    int mismatched_task_identifiers_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
    int mismatched_event_identifiers_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

    std::unique_ptr<MessageLoopObserver> message_loop_observer_
        GUARDED_BY_CONTEXT(sequence_checker_);
  };

  const std::string histogram_prefix_;
  const std::string main_source_name_;
  const bool consume_secondary_sources_;
  const bool register_first_idle_callback_;
  const bool observe_application_status_;

  SEQUENCE_CHECKER(main_sequence_checker_);

  std::unique_ptr<ResponsivenessCalculatorDelegate> delegate_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
  CallbackListSubscription idle_callback_subscription_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
  std::unique_ptr<Calculator> calculator_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
  std::unique_ptr<ThreadObserver> main_thread_observer_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
};

}  // namespace responsiveness
}  // namespace base

#endif  // BASE_TASK_RESPONSIVENESS_WATCHER_H_
