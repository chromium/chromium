// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/task_queue_web_view.h"

#include <memory>
#include <utility>

#include "android_webview/common/aw_features.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/no_destructor.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"

namespace android_webview {

namespace {

base::ThreadLocalBoolean* GetAllowGL() {
  static base::NoDestructor<base::ThreadLocalBoolean> allow_gl;
  return allow_gl.get();
}

// This task queue is used when the client and gpu service runs on the same
// thread (render thread). It has some simple logic to avoid reentrancy; in most
// cases calling schedule will actually run the task immediately.
class TaskQueueSingleThread : public TaskQueueWebView {
 public:
  TaskQueueSingleThread();
  ~TaskQueueSingleThread() override = default;

  // TaskQueueWebView overrides.
  void ScheduleTask(base::OnceClosure task, bool out_of_order) override;
  void ScheduleOrRetainTask(base::OnceClosure task) override;
  void ScheduleIdleTask(base::OnceClosure task) override;
  void ScheduleClientTask(base::OnceClosure task) override;
  void RunAllTasks() override;
  void InitializeVizThread(const scoped_refptr<base::SingleThreadTaskRunner>&
                               viz_task_runner) override;
  void ScheduleOnVizAndBlock(VizTask viz_task) override;

 private:
  // Flush the idle queue until it is empty.
  void PerformAllIdleWork();
  void RunTasks();

  // All access to task queue should happen on a single thread.
  THREAD_CHECKER(task_queue_thread_checker_);
  base::circular_deque<base::OnceClosure> tasks_;
  base::queue<base::OnceClosure> idle_tasks_;
  base::queue<base::OnceClosure> client_tasks_;

  bool inside_run_tasks_ = false;
  bool inside_run_idle_tasks_ = false;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueSingleThread);
};

TaskQueueSingleThread::TaskQueueSingleThread() {
  DETACH_FROM_THREAD(task_queue_thread_checker_);
}

void TaskQueueSingleThread::ScheduleTask(base::OnceClosure task,
                                         bool out_of_order) {
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  LOG_IF(FATAL, !GetAllowGL()->Get())
      << "ScheduleTask outside of ScopedAllowGL";
  if (out_of_order)
    tasks_.emplace_front(std::move(task));
  else
    tasks_.emplace_back(std::move(task));
  RunTasks();
}

void TaskQueueSingleThread::ScheduleOrRetainTask(base::OnceClosure task) {
  ScheduleTask(std::move(task), false);
}

void TaskQueueSingleThread::ScheduleIdleTask(base::OnceClosure task) {
  LOG_IF(FATAL, !GetAllowGL()->Get())
      << "ScheduleDelayedWork outside of ScopedAllowGL";
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  idle_tasks_.push(std::move(task));
}

void TaskQueueSingleThread::ScheduleClientTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  client_tasks_.emplace(std::move(task));
}

void TaskQueueSingleThread::RunTasks() {
  TRACE_EVENT0("android_webview", "TaskQueueSingleThread::RunTasks");
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  if (inside_run_tasks_)
    return;
  base::AutoReset<bool> inside(&inside_run_tasks_, true);
  while (tasks_.size()) {
    std::move(tasks_.front()).Run();
    tasks_.pop_front();
  }
}

void TaskQueueSingleThread::RunAllTasks() {
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  RunTasks();
  PerformAllIdleWork();
  DCHECK(tasks_.empty());
  DCHECK(idle_tasks_.empty());

  // Client tasks may generate more service tasks, so run this
  // in a loop.
  while (!client_tasks_.empty()) {
    base::queue<base::OnceClosure> local_client_tasks;
    local_client_tasks.swap(client_tasks_);
    while (!local_client_tasks.empty()) {
      std::move(local_client_tasks.front()).Run();
      local_client_tasks.pop();
    }

    RunTasks();
    PerformAllIdleWork();
    DCHECK(tasks_.empty());
    DCHECK(idle_tasks_.empty());
  }
}

void TaskQueueSingleThread::PerformAllIdleWork() {
  TRACE_EVENT0("android_webview", "TaskQueueWebview::PerformAllIdleWork");
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  if (inside_run_idle_tasks_)
    return;
  base::AutoReset<bool> inside(&inside_run_idle_tasks_, true);
  while (idle_tasks_.size() > 0) {
    base::OnceClosure task = std::move(idle_tasks_.front());
    idle_tasks_.pop();
    std::move(task).Run();
  }
}

void TaskQueueSingleThread::InitializeVizThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& viz_task_runner) {
  NOTREACHED();
}

void TaskQueueSingleThread::ScheduleOnVizAndBlock(VizTask viz_task) {
  NOTREACHED();
}

// This class is used with kVizForWebView. The client is the single viz
// thread and the gpu service runs on the render thread. Render thread is
// allowed to block on the viz thread, but not the other way around. This
// achieves viz scheduling tasks to gpu by first blocking render thread
// on the viz thread so render thread is ready to receive and run tasks.
//
// This class does not implement methods only needed by command buffer.
// It does not reply on ScopedAllowGL either.
class TaskQueueViz : public TaskQueueWebView {
 public:
  TaskQueueViz();
  ~TaskQueueViz() override;

  // TaskQueueWebView overrides.
  void ScheduleTask(base::OnceClosure task, bool out_of_order) override;
  void ScheduleOrRetainTask(base::OnceClosure task) override;
  void ScheduleIdleTask(base::OnceClosure task) override;
  void ScheduleClientTask(base::OnceClosure task) override;
  void RunAllTasks() override;
  void InitializeVizThread(const scoped_refptr<base::SingleThreadTaskRunner>&
                               viz_task_runner) override;
  void ScheduleOnVizAndBlock(VizTask viz_task) override;

 private:
  void RunOnViz(VizTask viz_task);
  void SignalDone();
  void EmplaceTask(base::OnceClosure task);

  scoped_refptr<base::SingleThreadTaskRunner> viz_task_runner_;

  // Only accessed on viz thread.
  bool allow_schedule_task_ = false;

  base::Lock lock_;
  base::ConditionVariable condvar_{&lock_};
  bool done_ GUARDED_BY(lock_) = true;
  base::circular_deque<base::OnceClosure> tasks_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(TaskQueueViz);
};

TaskQueueViz::TaskQueueViz() = default;
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
  NOTREACHED();
}

void TaskQueueViz::ScheduleClientTask(base::OnceClosure task) {
  DCHECK(viz_task_runner_);
  viz_task_runner_->PostTask(FROM_HERE, std::move(task));
}

void TaskQueueViz::RunAllTasks() {
  // Intentional no-op.
}

void TaskQueueViz::InitializeVizThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& viz_task_runner) {
  DCHECK(!viz_task_runner_);
  viz_task_runner_ = viz_task_runner;
}

void TaskQueueViz::ScheduleOnVizAndBlock(VizTask viz_task) {
  TRACE_EVENT0("android_webview", "ScheduleOnVizAndBlock");

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
    base::AutoLock lock(lock_);
    while (!done_ || !tasks_.empty()) {
      if (tasks_.empty())
        condvar_.Wait();
      base::circular_deque<base::OnceClosure> tasks;
      tasks.swap(tasks_);
      {
        base::AutoUnlock unlock(lock_);
        if (!tasks.empty()) {
          TRACE_EVENT0("android_webview", "RunTasks");
          while (tasks.size()) {
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

ScopedAllowGL::ScopedAllowGL() {
  DCHECK(!GetAllowGL()->Get());
  GetAllowGL()->Set(true);
}

ScopedAllowGL::~ScopedAllowGL() {
  TaskQueueWebView* service = TaskQueueWebView::GetInstance();
  DCHECK(service);
  service->RunAllTasks();
  GetAllowGL()->Set(false);
}

// static
TaskQueueWebView* TaskQueueWebView::GetInstance() {
  static TaskQueueWebView* task_queue =
      ::features::IsUsingVizForWebView()
          ? static_cast<TaskQueueWebView*>(new TaskQueueViz)
          : static_cast<TaskQueueWebView*>(new TaskQueueSingleThread);
  return task_queue;
}

}  // namespace android_webview
