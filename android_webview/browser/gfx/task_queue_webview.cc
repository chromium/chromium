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
#include "gpu/command_buffer/service/scheduler.h"

namespace android_webview {

TaskQueueWebView::Sequence::Sequence(gpu::Scheduler* scheduler)
    : gpu::TaskGraph::Sequence(scheduler->task_graph(),
                               base::DoNothing(),
                               /*validation_runner=*/{}),
      scheduler_(scheduler) {}

void TaskQueueWebView::Sequence::RunAllTasks() {
  base::AutoLock auto_lock(lock());
  while (!tasks_.empty()) {
    // Synchronously wait for the fences of the front task.
    while (!IsFrontTaskUnblocked()) {
      gpu::SyncToken sync_token = wait_fences_.begin()->sync_token;
      uint32_t order_num = wait_fences_.begin()->order_num;
      gpu::SequenceId release_sequence_id =
          wait_fences_.begin()->release_sequence_id;

      // Must unlock the task graph lock, otherwise it will deadlock when
      // calling into scheduler to update sequence priority, or when blocking on
      // `completion` waiting for other tasks to release fences.
      //
      // Manually release and re-acquire the lock, because locking annotation
      // used on ValidateSequenceTaskFenceDeps() doesn't recognize
      // base::AutoUnlock.
      lock().Release();

      base::WaitableEvent completion;
      if (task_graph_->sync_point_manager()->Wait(
              sync_token, sequence_id_, order_num,
              base::BindOnce(&base::WaitableEvent::Signal,
                             base::Unretained(&completion)))) {
        TRACE_EVENT1("android_webview",
                     "TaskQueueWebView::Sequence::RunAllTasks::WaitSyncToken",
                     "sequence_id", release_sequence_id.value());
        gpu::Scheduler::ScopedSetSequencePriority waiting(
            scheduler_, release_sequence_id, gpu::SchedulingPriority::kHigh);

        if (task_graph_->graph_validation_enabled()) {
          while (!completion.TimedWait(gpu::TaskGraph::kMinValidationDelay)) {
            task_graph_->ValidateSequenceTaskFenceDeps(this);
          }
        } else {
          completion.Wait();
        }
      }

      lock().Acquire();
    }

    // Run the front task.
    base::OnceClosure task_closure;
    uint32_t order_num = BeginTask(&task_closure);
    gpu::SyncToken release = current_task_release_;

    {
      base::AutoUnlock auto_unlock(lock());
      order_data()->BeginProcessingOrderNumber(order_num);

      std::move(task_closure).Run();

      if (order_data()->IsProcessingOrderNumber()) {
        if (release.HasData()) {
          task_graph_->sync_point_manager()->EnsureFenceSyncReleased(
              release, gpu::ReleaseCause::kTaskCompletionRelease);
        }

        order_data()->FinishProcessingOrderNumber(order_num);
      }
    }

    FinishTask();
  }
}

bool TaskQueueWebView::Sequence::HasTasksAcquiringLock() const {
  base::AutoLock auto_lock(lock());
  return HasTasks();
}

uint32_t TaskQueueWebView::Sequence::AddTaskAcquiringLock(
    base::OnceClosure task_closure,
    std::vector<gpu::SyncToken> wait_fences,
    const gpu::SyncToken& release,
    ReportingCallback report_callback) {
  base::AutoLock auto_lock(lock());
  return AddTask(std::move(task_closure), std::move(wait_fences), release,
                 std::move(report_callback));
}

gpu::ScopedSyncPointClientState
TaskQueueWebView::Sequence::CreateSyncPointClientStateAcquiringLock(
    gpu::CommandBufferNamespace namespace_id,
    gpu::CommandBufferId command_buffer_id) {
  base::AutoLock auto_lock(lock());
  return CreateSyncPointClientState(namespace_id, command_buffer_id);
}

// static
TaskQueueWebView* TaskQueueWebView::GetInstance() {
  static TaskQueueWebView* task_queue = new TaskQueueWebView;
  return task_queue;
}

TaskQueueWebView::~TaskQueueWebView() {
  base::AutoLock lock(lock_);
  if (sequence_) {
    GpuServiceWebView::GetInstance()
        ->scheduler()
        ->task_graph()
        ->DestroySequence(sequence_->sequence_id());
    sequence_ = nullptr;
  }
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
    while (!done_ || (sequence_ && sequence_->HasTasksAcquiringLock())) {
      while (!done_ && (!sequence_ || !sequence_->HasTasksAcquiringLock())) {
        condvar_.Wait();
      }

      if (sequence_) {
        base::AutoUnlock unlock(lock_);
        TRACE_EVENT0("android_webview", "RunTasks");
        sequence_->RunAllTasks();
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
  if (sequence_) {
    return;
  }

  auto sequence =
      std::make_unique<Sequence>(GpuServiceWebView::GetInstance()->scheduler());
  sequence_ = sequence.get();

  GpuServiceWebView::GetInstance()->scheduler()->task_graph()->AddSequence(
      std::move(sequence));
}

gpu::SequenceId TaskQueueWebView::GetSequenceId() {
  base::AutoLock lock(lock_);
  return sequence_->sequence_id();
}

void TaskQueueWebView::ScheduleTask(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences,
    const gpu::SyncToken& release,
    ReportingCallback report_callback) {
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
    ReportingCallback report_callback) {
  DCHECK(viz_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(lock_);
  sequence_->AddTaskAcquiringLock(std::move(task), std::move(sync_token_fences),
                                  release, std::move(report_callback));

  condvar_.Signal();
}

void TaskQueueWebView::ScheduleIdleTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  DCHECK(inside_schedule_on_viz_and_block_);

  base::AutoLock lock(lock_);
  sequence_->AddTaskAcquiringLock(std::move(task), /*wait_fences=*/{},
                                  gpu::SyncToken(),
                                  /*report_callback=*/{});

  condvar_.Signal();
}

gpu::ScopedSyncPointClientState TaskQueueWebView::CreateSyncPointClientState(
    gpu::CommandBufferNamespace namespace_id,
    gpu::CommandBufferId command_buffer_id) {
  base::AutoLock lock(lock_);
  return sequence_->CreateSyncPointClientStateAcquiringLock(namespace_id,
                                                            command_buffer_id);
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
