// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEBVIEW_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/single_task_sequence.h"
#include "gpu/command_buffer/service/task_graph.h"

namespace gpu {
class Scheduler;
class ScopedSyncPointClientState;
struct SyncToken;
}  // namespace gpu

namespace android_webview {

// In WebView, there is a single task queue that runs all tasks instead of
// thread task runners. This is the class actually scheduling and running tasks
// for WebView. This is used by both CommandBuffer and SkiaDDL.
//
// The client is the single viz thread and the gpu service runs on the render
// thread. Render thread is allowed to block on the viz thread, but not the
// other way around. This achieves viz scheduling tasks to gpu by first blocking
// render thread on the viz thread so render thread is ready to receive and run
// tasks.
//
// Lifetime: Singleton
class TaskQueueWebView {
 public:
  // Static method that makes sure this is only one copy of this class.
  static TaskQueueWebView* GetInstance();

  TaskQueueWebView(const TaskQueueWebView&) = delete;
  TaskQueueWebView& operator=(const TaskQueueWebView&) = delete;

  ~TaskQueueWebView();

  void InitializeVizThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& viz_task_runner);

  // The calling OnceClosure unblocks the render thread, and disallows further
  // calls to ScheduleTask.
  using VizTask = base::OnceCallback<void(base::OnceClosure)>;
  void ScheduleOnVizAndBlock(VizTask viz_task);

  // Used to post task to client thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetClientTaskRunner();

  // Unit tests can switch render thread.
  void ResetRenderThreadForTesting() {
    DETACH_FROM_THREAD(render_thread_checker_);
  }

  // Lazily initializes internal sequence state if hasn't been done yet.
  // The following public methods can only be called after this method is
  // called.
  //
  // Calls after the first one are no-ops.
  void EnsureSequenceInitialized();

  gpu::SequenceId GetSequenceId();

  using ReportingCallback = gpu::SingleTaskSequence::ReportingCallback;
  void ScheduleTask(base::OnceClosure task,
                    std::vector<gpu::SyncToken> sync_token_fences,
                    const gpu::SyncToken& release,
                    ReportingCallback report_callback);
  void ScheduleOrRetainTask(base::OnceClosure task,
                            std::vector<gpu::SyncToken> sync_token_fences,
                            const gpu::SyncToken& release,
                            ReportingCallback report_callback);

  // Called to schedule delayed tasks.
  void ScheduleIdleTask(base::OnceClosure task);

  [[nodiscard]] gpu::ScopedSyncPointClientState CreateSyncPointClientState(
      gpu::CommandBufferNamespace namespace_id,
      gpu::CommandBufferId command_buffer_id);

 private:
  class Sequence : public gpu::TaskGraph::Sequence {
   public:
    explicit Sequence(gpu::Scheduler* scheduler);

    void RunAllTasks() LOCKS_EXCLUDED(lock());

    // The following methods acquire the TaskGraph lock before calling the
    // corresponding methods in gpu::TaskGraph::Sequence.
    bool HasTasksAcquiringLock() const LOCKS_EXCLUDED(lock());
    uint32_t AddTaskAcquiringLock(base::OnceClosure task_closure,
                                  std::vector<gpu::SyncToken> wait_fences,
                                  const gpu::SyncToken& release,
                                  ReportingCallback report_callback)
        LOCKS_EXCLUDED(lock());
    [[nodiscard]] gpu::ScopedSyncPointClientState
    CreateSyncPointClientStateAcquiringLock(
        gpu::CommandBufferNamespace namespace_id,
        gpu::CommandBufferId command_buffer_id) LOCKS_EXCLUDED(lock());

   private:
    const raw_ptr<gpu::Scheduler> scheduler_ = nullptr;
  };

  TaskQueueWebView();

  void RunOnViz(VizTask viz_task);
  void SignalDone();

  scoped_refptr<base::SingleThreadTaskRunner> viz_task_runner_;
  THREAD_CHECKER(render_thread_checker_);

  // Only accessed on viz thread.
  bool allow_schedule_task_ = false;

  // Only accessed on render thread.
  bool inside_schedule_on_viz_and_block_ = false;

  base::Lock lock_;
  base::ConditionVariable condvar_{&lock_};
  bool done_ GUARDED_BY(lock_) = true;

  raw_ptr<Sequence> sequence_ GUARDED_BY(lock_) = nullptr;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEBVIEW_H_
