// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_RESPONSIVENESS_WATCHER_H_
#define BASE_TASK_RESPONSIVENESS_WATCHER_H_

#include <stdint.h>

#include <memory>
#include <variant>
#include <vector>

#include "base/base_export.h"
#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace base {

struct PendingTask;
class SequencedTaskRunner;

namespace responsiveness {

class Calculator;
class MessageLoopObserver;

class BASE_EXPORT Watcher : public base::RefCounted<Watcher> {
 public:
  explicit Watcher(scoped_refptr<SequencedTaskRunner> io_task_runner);
  void SetUp();
  virtual void Destroy();

  // Must be invoked once-and-only-once, after SetUp(), the first time
  // MainMessageLoopRun() reaches idle (i.e. done running all tasks queued
  // during startup). This will be used as a signal for the true end of
  // "startup" and the beginning of recording Browser.MainThreadsCongestion.
  void OnFirstIdle();

 protected:
  friend class base::RefCounted<Watcher>;

  // Exposed for tests.
  virtual std::unique_ptr<Calculator> CreateCalculator();
  virtual void RegisterMessageLoopObserverUI();
  virtual void RegisterMessageLoopObserverIO();

  virtual ~Watcher();

  void SetUpOnIOThread();
  void TearDownOnUIThread();
  void TearDownOnIOThread();

  void WillRunTaskOnUIThread(const base::PendingTask* task,
                             bool was_blocked_or_low_priority);
  void DidRunTaskOnUIThread(const base::PendingTask* task);

  void WillRunTaskOnIOThread(const base::PendingTask* task,
                             bool was_blocked_or_low_priority);
  void DidRunTaskOnIOThread(const base::PendingTask* task);

  void WillRunEventOnUIThread(uintptr_t opaque_identifier);
  void DidRunEventOnUIThread(uintptr_t opaque_identifier);

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

  // Common implementations for the thread-specific methods.
  void WillRunTask(const base::PendingTask* task,
                   bool was_blocked_or_low_priority,
                   std::vector<Metadata>* currently_running_metadata);

  // TODO(crbug.com/40287434): After the "ReduceCpuUtilization2" feature is
  // cleaned up (~January 2025), remove the std::variant in favor of a
  // base::FunctionRef.
  using TaskOrEventFinishedSignature = void(base::TimeTicks,
                                            base::TimeTicks,
                                            base::TimeTicks);
  using TaskOrEventFinishedCallback =
      base::FunctionRef<TaskOrEventFinishedSignature>;
  // |callback| will either be synchronously invoked, or else never invoked.
  void DidRunTask(const base::PendingTask* task,
                  std::vector<Metadata>* currently_running_metadata,
                  int* mismatched_task_identifiers,
                  TaskOrEventFinishedCallback callback);

  scoped_refptr<SequencedTaskRunner> io_task_runner_;

  // The following members are all affine to the UI thread.
  std::unique_ptr<Calculator> calculator_;
  std::unique_ptr<MessageLoopObserver> message_loop_observer_ui_;
  bool destroy_was_called_ = false;
  SEQUENCE_CHECKER(ui_sequence_checker_);

  // Metadata for currently running tasks and events on the UI thread.
  std::vector<Metadata> currently_running_metadata_ui_;

  // Task identifiers should only be mismatched once, since the Watcher may
  // register itself during a Task execution, and thus doesn't capture the
  // initial WillRunTask() callback.
  int mismatched_task_identifiers_ui_ = 0;

  // Event identifiers should be mismatched at most once, since the Watcher may
  // register itself during an event execution, and thus doesn't capture the
  // initial WillRunEventOnUIThread callback.
  int mismatched_event_identifiers_ui_ = 0;

  // The following members are all affine to the IO thread.
  std::unique_ptr<MessageLoopObserver> message_loop_observer_io_;
  std::vector<Metadata> currently_running_metadata_io_;
  int mismatched_task_identifiers_io_ = 0;
  SEQUENCE_CHECKER(io_sequence_checker_);

  // The implementation of this class guarantees that |calculator_io_| will be
  // non-nullptr and point to a valid object any time it is used on the IO
  // thread. To ensure this, the first task that this class posts onto the IO
  // thread sets |calculator_io_|. On destruction, this class first tears down
  // all consumers of |calculator_io_|, and then clears the member and destroys
  // Calculator.
  // `calculator_io_` is not a raw_ptr<...> because Calculator isn't supported
  // in raw_ptr for performance reasons. See crbug.com/1287151.
  RAW_PTR_EXCLUSION Calculator* calculator_io_ = nullptr;
};

}  // namespace responsiveness
}  // namespace base

#endif  // BASE_TASK_RESPONSIVENESS_WATCHER_H_
