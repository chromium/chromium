// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/task_queue_webview.h"

#include <memory>
#include <utility>

#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/blocking_sequence_runner.h"
#include "gpu/command_buffer/service/scheduler.h"

namespace android_webview {

// static
TaskQueueWebView* TaskQueueWebView::GetInstance() {
  static TaskQueueWebView* task_queue = new TaskQueueWebView;
  return task_queue;
}

TaskQueueWebView::~TaskQueueWebView() {
  base::AutoLock lock(lock_);
  blocking_sequence_runner_.reset();
}

void TaskQueueWebView::InitializeVizThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& viz_task_runner) {
  DCHECK(!viz_task_runner_);
  viz_task_runner_ = viz_task_runner;
}

void TaskQueueWebView::ScheduleOnVizAndBlock(VizTask viz_task) {
  TRACE_EVENT0("android_webview", "ScheduleOnVizAndBlock");
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Android.WebView.ScheduleOnVizAndBlockTime");

  // Expected behavior is |viz_task| on the viz thread. From |viz_task| until
  // the done closure is called (which may not be in the viz_task), viz thread
  // is allowed to call ScheduleTask.
  //
  // Implementation uses a normal run-loop like logic. The done closure marks
  // |done_| true, and run loop exists when |done_| is true *and* the task queue
  // is empty. A condition variable is signaled when |done_| is set or when
  // something is appended to the task queue.
  {
    base::AutoLock lock(lock_);
    DCHECK(done_);
    done_ = false;
  }

  // Unretained safe because this object is never deleted.
  viz_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TaskQueueWebView::RunOnViz,
                                base::Unretained(this), std::move(viz_task)));

  {
    DCHECK(!inside_schedule_on_viz_and_block_);
    base::AutoReset<bool> inside_bf(&inside_schedule_on_viz_and_block_, true);

    base::AutoLock lock(lock_);
    while (!done_ || (blocking_sequence_runner_ &&
                      blocking_sequence_runner_->HasTasks())) {
      while (!done_ && (!blocking_sequence_runner_ ||
                        !blocking_sequence_runner_->HasTasks())) {
        condvar_.Wait();
      }

      if (blocking_sequence_runner_) {
        base::AutoUnlock unlock(lock_);
        TRACE_EVENT0("android_webview", "RunTasks");
        blocking_sequence_runner_->RunAllTasks();
      }
    }
    DCHECK(done_);
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
TaskQueueWebView::GetClientTaskRunner() {
  DCHECK(viz_task_runner_);
  return viz_task_runner_;
}

void TaskQueueWebView::EnsureSequenceInitialized() {
  base::AutoLock lock(lock_);
  if (blocking_sequence_runner_) {
    return;
  }

  blocking_sequence_runner_ = std::make_unique<gpu::BlockingSequenceRunner>(
      GpuServiceWebView::GetInstance()->scheduler());
}

gpu::SequenceId TaskQueueWebView::GetSequenceId() {
  base::AutoLock lock(lock_);
  return blocking_sequence_runner_->GetSequenceId();
}

void TaskQueueWebView::ScheduleTask(
    gpu::TaskCallback task,
    std::vector<gpu::SyncToken> sync_token_fences,
    const gpu::SyncToken& release,
    gpu::ReportingCallback report_callback) {
  TRACE_EVENT0("android_webview", "ScheduleTask");
  DCHECK(viz_task_runner_->BelongsToCurrentThread());

  DCHECK(allow_schedule_task_);

  base::AutoLock lock(lock_);
  blocking_sequence_runner_->AddTask(std::move(task),
                                     std::move(sync_token_fences), release,
                                     std::move(report_callback));

  condvar_.Signal();
}

void TaskQueueWebView::ScheduleTask(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences,
    const gpu::SyncToken& release,
    gpu::ReportingCallback report_callback) {
  TRACE_EVENT0("android_webview", "ScheduleTask");
  DCHECK(viz_task_runner_->BelongsToCurrentThread());

  DCHECK(allow_schedule_task_);
  // The two branches end up doing the exact same thing only because retain
  // can use the same task queue. The code says the intention which is
  // |ScheduleOrRetainTask| behaves the same as |ScheduleTask| if
  // |allow_schedule_task_| is true.
  // Sharing the queue makes it clear |ScheduleTask| and
  // |ScheduleOrRetainTask| but however has a non-practical risk of
  // live-locking the render thread.
  ScheduleOrRetainTask(std::move(task), std::move(sync_token_fences), release,
                       std::move(report_callback));
}

void TaskQueueWebView::ScheduleOrRetainTask(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences,
    const gpu::SyncToken& release,
    gpu::ReportingCallback report_callback) {
  DCHECK(viz_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(lock_);
  blocking_sequence_runner_->AddTask(std::move(task),
                                     std::move(sync_token_fences), release,
                                     std::move(report_callback));

  condvar_.Signal();
}

void TaskQueueWebView::ScheduleIdleTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  DCHECK(inside_schedule_on_viz_and_block_);

  base::AutoLock lock(lock_);
  blocking_sequence_runner_->AddTask(std::move(task), /*wait_fences=*/{},
                                     gpu::SyncToken(),
                                     /*report_callback=*/{});

  condvar_.Signal();
}

gpu::ScopedSyncPointClientState TaskQueueWebView::CreateSyncPointClientState(
    gpu::CommandBufferNamespace namespace_id,
    gpu::CommandBufferId command_buffer_id) {
  base::AutoLock lock(lock_);
  return blocking_sequence_runner_->CreateSyncPointClientState(
      namespace_id, command_buffer_id);
}

TaskQueueWebView::TaskQueueWebView() {
  DETACH_FROM_THREAD(render_thread_checker_);
}

void TaskQueueWebView::RunOnViz(VizTask viz_task) {
  DCHECK(viz_task_runner_->BelongsToCurrentThread());
  DCHECK(!allow_schedule_task_);
  allow_schedule_task_ = true;
  // Unretained safe because this object is never deleted.
  std::move(viz_task).Run(
      base::BindOnce(&TaskQueueWebView::SignalDone, base::Unretained(this)));
}

void TaskQueueWebView::SignalDone() {
  DCHECK(viz_task_runner_->BelongsToCurrentThread());
  DCHECK(allow_schedule_task_);
  allow_schedule_task_ = false;

  base::AutoLock lock(lock_);
  DCHECK(!done_);
  done_ = true;
  condvar_.Signal();
}

}  // namespace android_webview
