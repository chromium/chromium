// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/task_queue_webview.h"

#include <memory>
#include <utility>

#include "android_webview/common/aw_features.h"
#include "base/auto_reset.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"

namespace android_webview {

namespace {

// The client is the single viz thread and the gpu service runs on the render
// thread. Render thread is allowed to block on the viz thread, but not the
// other way around. This achieves viz scheduling tasks to gpu by first blocking
// render thread on the viz thread so render thread is ready to receive and run
// tasks.
//
// Lifetime: Singleton
class TaskQueueViz : public TaskQueueWebView {
 public:
  TaskQueueViz();

  TaskQueueViz(const TaskQueueViz&) = delete;
  TaskQueueViz& operator=(const TaskQueueViz&) = delete;

  ~TaskQueueViz() override;

  // TaskQueueWebView overrides.
  void ScheduleTask(base::OnceClosure task, bool out_of_order) override;
  void ScheduleOrRetainTask(base::OnceClosure task) override;
  void ScheduleIdleTask(base::OnceClosure task) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetClientTaskRunner() override;
  void InitializeVizThread(const scoped_refptr<base::SingleThreadTaskRunner>&
                               viz_task_runner) override;
  void ScheduleOnVizAndBlock(VizTask viz_task) override;
  void ResetRenderThreadForTesting() override {
    DETACH_FROM_THREAD(render_thread_checker_);
  }

 private:
  void RunOnViz(VizTask viz_task);
  void SignalDone();
  void EmplaceTask(base::OnceClosure task);

  scoped_refptr<base::SingleThreadTaskRunner> viz_task_runner_;
  THREAD_CHECKER(render_thread_checker_);

  // Only accessed on viz thread.
  bool allow_schedule_task_ = false;

  // Only accessed on render thread.
  bool inside_schedule_on_viz_and_block_ = false;

  base::Lock lock_;
  base::ConditionVariable condvar_{&lock_};
  bool done_ GUARDED_BY(lock_) = true;
  base::circular_deque<base::OnceClosure> tasks_ GUARDED_BY(lock_);
};

TaskQueueViz::TaskQueueViz() {
  DETACH_FROM_THREAD(render_thread_checker_);
}

TaskQueueViz::~TaskQueueViz() = default;

void TaskQueueViz::ScheduleTask(base::OnceClosure task, bool out_of_order) {
  TRACE_EVENT0("android_webview", "ScheduleTask");
  DCHECK(viz_task_runner_->BelongsToCurrentThread());
  DCHECK(allow_schedule_task_);
  // |out_of_order| is not needed by TaskForwardingSequence. Not supporting
  // it allows slightly more efficient swapping the task queue in
  // ScheduleOnVizAndBlock .
  DCHECK(!out_of_order);
  EmplaceTask(std::move(task));
}

void TaskQueueViz::ScheduleOrRetainTask(base::OnceClosure task) {
  DCHECK(viz_task_runner_->BelongsToCurrentThread());
  // The two branches end up doing the exact same thing only because retain can
  // use the same task queue. The code says the intention which is
  // |ScheduleOrRetainTask| behaves the same as |ScheduleTask| if
  // |allow_schedule_task_| is true.
  // Sharing the queue makes it clear |ScheduleTask| and |ScheduleOrRetainTask|
  // but however has a non-practical risk of live-locking the render thread.
  if (allow_schedule_task_) {
    ScheduleTask(std::move(task), false);
    return;
  }
  EmplaceTask(std::move(task));
}

void TaskQueueViz::EmplaceTask(base::OnceClosure task) {
  base::AutoLock lock(lock_);
  tasks_.emplace_back(std::move(task));
  condvar_.Signal();
}

void TaskQueueViz::ScheduleIdleTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  DCHECK(inside_schedule_on_viz_and_block_);
  EmplaceTask(std::move(task));
}

scoped_refptr<base::SingleThreadTaskRunner>
TaskQueueViz::GetClientTaskRunner() {
  DCHECK(viz_task_runner_);
  return viz_task_runner_;
}

void TaskQueueViz::InitializeVizThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& viz_task_runner) {
  DCHECK(!viz_task_runner_);
  viz_task_runner_ = viz_task_runner;
}

void TaskQueueViz::ScheduleOnVizAndBlock(VizTask viz_task) {
  TRACE_EVENT0("android_webview", "ScheduleOnVizAndBlock");
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Android.WebView.ScheduleOnVizAndBlockTime");

  // Expected behavior is |viz_task| on the viz thread. From |viz_task| until
  // the done closure is called (which may not be in the viz_task), viz thread
  // is allowed to call ScheduleTask.
  //
  // Implementation is uses a normal run-loop like logic. The done closure
  // marks |done_| true, and run loop exists when |done_| is true *and* the task
  // queue is empty. A condition variable is signaled when |done_| is set or
  // when something is appended to the task queue.
  {
    base::AutoLock lock(lock_);
    DCHECK(done_);
    done_ = false;
  }

  // Unretained safe because this object is never deleted.
  viz_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TaskQueueViz::RunOnViz, base::Unretained(this),
                                std::move(viz_task)));

  {
    DCHECK(!inside_schedule_on_viz_and_block_);
    base::AutoReset<bool> inside_bf(&inside_schedule_on_viz_and_block_, true);

    base::AutoLock lock(lock_);
    while (!done_ || !tasks_.empty()) {
      while (!done_ && tasks_.empty())
        condvar_.Wait();
      if (!tasks_.empty()) {
        base::circular_deque<base::OnceClosure> tasks;
        tasks.swap(tasks_);
        {
          base::AutoUnlock unlock(lock_);
          TRACE_EVENT0("android_webview", "RunTasks");
          while (!tasks.empty()) {
            std::move(tasks.front()).Run();
            tasks.pop_front();
          }
        }
      }
    }
    DCHECK(done_);
  }
}

void TaskQueueViz::RunOnViz(VizTask viz_task) {
  DCHECK(viz_task_runner_->BelongsToCurrentThread());
  DCHECK(!allow_schedule_task_);
  allow_schedule_task_ = true;
  // Unretained safe because this object is never deleted.
  std::move(viz_task).Run(
      base::BindOnce(&TaskQueueViz::SignalDone, base::Unretained(this)));
}

void TaskQueueViz::SignalDone() {
  DCHECK(viz_task_runner_->BelongsToCurrentThread());
  DCHECK(allow_schedule_task_);
  allow_schedule_task_ = false;

  base::AutoLock lock(lock_);
  DCHECK(!done_);
  done_ = true;
  condvar_.Signal();
}

}  // namespace

// static
TaskQueueWebView* TaskQueueWebView::GetInstance() {
  static TaskQueueWebView* task_queue =
      static_cast<TaskQueueWebView*>(new TaskQueueViz);
  return task_queue;
}

}  // namespace android_webview
