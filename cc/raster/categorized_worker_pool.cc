// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/categorized_worker_pool.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/base/switches.h"
#include "cc/raster/task_category.h"

namespace cc {
namespace {

BASE_FEATURE(kUseCompositorJob,
             "UseCompositorJob",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Task categories running at normal thread priority.
constexpr TaskCategory kNormalThreadPriorityCategories[] = {
    TASK_CATEGORY_NONCONCURRENT_FOREGROUND, TASK_CATEGORY_FOREGROUND,
    TASK_CATEGORY_BACKGROUND_WITH_NORMAL_THREAD_PRIORITY};

// Task categories running at background thread priority.
constexpr TaskCategory kBackgroundThreadPriorityCategories[] = {
    TASK_CATEGORY_BACKGROUND};

// Foreground task categories.
constexpr TaskCategory kForegroundCategories[] = {
    TASK_CATEGORY_NONCONCURRENT_FOREGROUND, TASK_CATEGORY_FOREGROUND};

// Background task categories. Tasks in these categories cannot start running
// when a task with a category in |kForegroundCategories| is running or ready to
// run.
constexpr TaskCategory kBackgroundCategories[] = {
    TASK_CATEGORY_BACKGROUND,
    TASK_CATEGORY_BACKGROUND_WITH_NORMAL_THREAD_PRIORITY};

// A thread which forwards to CategorizedWorkerPool::Run with the runnable
// categories.
class CategorizedWorkerPoolThread : public base::SimpleThread {
 public:
  CategorizedWorkerPoolThread(
      const std::string& name_prefix,
      const Options& options,
      CategorizedWorkerPoolImpl* pool,
      std::vector<TaskCategory> categories,
      base::ConditionVariable* has_ready_to_run_tasks_cv)
      : SimpleThread(name_prefix, options),
        pool_(pool),
        categories_(categories),
        has_ready_to_run_tasks_cv_(has_ready_to_run_tasks_cv) {}

  // base::SimpleThread:
  void BeforeRun() override { pool_->ThreadWillRun(tid()); }

  void Run() override { pool_->Run(categories_, has_ready_to_run_tasks_cv_); }

 private:
  const raw_ptr<CategorizedWorkerPoolImpl> pool_;
  const std::vector<TaskCategory> categories_;
  const raw_ptr<base::ConditionVariable> has_ready_to_run_tasks_cv_;

  base::OnceCallback<void(base::PlatformThreadId)> backgrounding_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
};

scoped_refptr<CategorizedWorkerPool>& GetWorkerPool() {
  static base::NoDestructor<scoped_refptr<CategorizedWorkerPool>> worker_pool;
  return *worker_pool;
}

}  // namespace

// A sequenced task runner which posts tasks to a CategorizedWorkerPool.
class CategorizedWorkerPool::CategorizedWorkerPoolSequencedTaskRunner
    : public base::SequencedTaskRunner {
 public:
  explicit CategorizedWorkerPoolSequencedTaskRunner(
      TaskGraphRunner* task_graph_runner)
      : task_graph_runner_(task_graph_runner),
        namespace_token_(task_graph_runner->GenerateNamespaceToken()) {}

  // Overridden from base::TaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return PostNonNestableDelayedTask(from_here, std::move(task), delay);
  }

  // Overridden from base::SequencedTaskRunner:
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
    // for details.
    CHECK(task);
    base::AutoLock lock(lock_);

    // Remove completed tasks.
    DCHECK(completed_tasks_.empty());
    task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                              &completed_tasks_);

    tasks_.erase(tasks_.begin(), tasks_.begin() + completed_tasks_.size());

    tasks_.push_back(base::MakeRefCounted<ClosureTask>(std::move(task)));
    graph_.Reset();
    for (const auto& graph_task : tasks_) {
      int dependencies = 0;
      if (!graph_.nodes.empty()) {
        dependencies = 1;
      }

      // Treat any tasks that are enqueued through the SequencedTaskRunner as
      // FOREGROUND priority. We don't have enough information to know the
      // actual priority of such tasks, so we run them as soon as possible.
      TaskGraph::Node node(graph_task, TASK_CATEGORY_FOREGROUND,
                           0u /* priority */, dependencies);
      if (dependencies) {
        graph_.edges.push_back(
            TaskGraph::Edge(graph_.nodes.back().task.get(), node.task.get()));
      }
      graph_.nodes.push_back(std::move(node));
    }
    task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);
    completed_tasks_.clear();
    return true;
  }

  bool RunsTasksInCurrentSequence() const override { return true; }

 private:
  ~CategorizedWorkerPoolSequencedTaskRunner() override {
    {
      base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
      task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
    }
    task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                              &completed_tasks_);
  }

  // Lock to exclusively access all the following members that are used to
  // implement the SequencedTaskRunner interfaces.
  base::Lock lock_;

  raw_ptr<TaskGraphRunner> task_graph_runner_;
  // Namespace used to schedule tasks in the task graph runner.
  NamespaceToken namespace_token_;
  // List of tasks currently queued up for execution.
  Task::Vector tasks_;
  // Graph object used for scheduling tasks.
  TaskGraph graph_;
  // Cached vector to avoid allocation when getting the list of complete
  // tasks.
  Task::Vector completed_tasks_;
};

CategorizedWorkerPoolImpl::CategorizedWorkerPoolImpl(Delegate* delegate)
    : delegate_(delegate),
      has_task_for_normal_priority_thread_cv_(&lock_),
      has_task_for_background_priority_thread_cv_(&lock_),
      shutdown_(false) {
  // Declare the two ConditionVariables which are used by worker threads to
  // sleep-while-idle as such to avoid throwing off //base heuristics.
  has_task_for_normal_priority_thread_cv_.declare_only_used_while_idle();
  has_task_for_background_priority_thread_cv_.declare_only_used_while_idle();
}

CategorizedWorkerPoolImpl::~CategorizedWorkerPoolImpl() = default;

void CategorizedWorkerPoolImpl::Start(int max_concurrency_foreground) {
  DCHECK(threads_.empty());

  // |max_concurrency_foreground| normal threads and 1 background threads are
  // created.
  const size_t num_threads = max_concurrency_foreground + 1;
  threads_.reserve(num_threads);

  // Start |max_concurrency_foreground| normal priority threads, which run
  // foreground work and background work that cannot run at background thread
  // priority.
  std::vector<TaskCategory> normal_thread_prio_categories(
      std::begin(kNormalThreadPriorityCategories),
      std::end(kNormalThreadPriorityCategories));

  for (int i = 0; i < max_concurrency_foreground; i++) {
    auto thread = std::make_unique<CategorizedWorkerPoolThread>(
        base::StringPrintf("CompositorTileWorker%d", i + 1),
        base::SimpleThread::Options(), this, normal_thread_prio_categories,
        &has_task_for_normal_priority_thread_cv_);
    thread->StartAsync();
    threads_.push_back(std::move(thread));
  }

  // Start a single thread running at background thread priority.
  std::vector<TaskCategory> background_thread_prio_categories{
      std::begin(kBackgroundThreadPriorityCategories),
      std::end(kBackgroundThreadPriorityCategories)};

  base::SimpleThread::Options thread_options;
// TODO(crbug.com/40226019): Figure out whether !IS_MAC can be lifted here.
#if !BUILDFLAG(IS_MAC)
  thread_options.thread_type = base::ThreadType::kBackground;
#endif

  auto thread = std::make_unique<CategorizedWorkerPoolThread>(
      "CompositorTileWorkerBackground", thread_options, this,
      background_thread_prio_categories,
      &has_task_for_background_priority_thread_cv_);
  thread->StartAsync();
  threads_.push_back(std::move(thread));

  DCHECK_EQ(num_threads, threads_.size());
}

void CategorizedWorkerPoolImpl::Shutdown() {
  {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    WaitForTasksToFinishRunning(namespace_token_);
  }

  CollectCompletedTasks(namespace_token_, &completed_tasks_);
  // Shutdown raster threads.
  {
    base::AutoLock lock(lock_);

    DCHECK(!work_queue_.HasReadyToRunTasks());
    DCHECK(!work_queue_.HasAnyNamespaces());

    DCHECK(!shutdown_);
    shutdown_ = true;

    // Wake up all workers so they exit.
    has_task_for_normal_priority_thread_cv_.Broadcast();
    has_task_for_background_priority_thread_cv_.Broadcast();
  }
  while (!threads_.empty()) {
    threads_.back()->Join();
    threads_.pop_back();
  }
}

void CategorizedWorkerPoolImpl::ThreadWillRun(base::PlatformThreadId tid) {
  if (delegate_) {
    delegate_->NotifyThreadWillRun(tid);
  }
}

// Overridden from base::TaskRunner:
bool CategorizedWorkerPoolImpl::PostDelayedTask(const base::Location& from_here,
                                                base::OnceClosure task,
                                                base::TimeDelta delay) {
  base::AutoLock lock(lock_);

  // Remove completed tasks.
  DCHECK(completed_tasks_.empty());
  CollectCompletedTasksWithLockAcquired(namespace_token_, &completed_tasks_);

  std::erase_if(tasks_, [this](const scoped_refptr<Task>& e)
                            EXCLUSIVE_LOCKS_REQUIRED(lock_) {
                              return base::Contains(this->completed_tasks_, e);
                            });

  tasks_.push_back(base::MakeRefCounted<ClosureTask>(std::move(task)));
  graph_.Reset();
  for (const auto& graph_task : tasks_) {
    // Delayed tasks are assigned FOREGROUND category, ensuring that they run as
    // soon as possible once their delay has expired.
    graph_.nodes.push_back(
        TaskGraph::Node(graph_task.get(), TASK_CATEGORY_FOREGROUND,
                        0u /* priority */, 0u /* dependencies */));
  }

  ScheduleTasksWithLockAcquired(namespace_token_, &graph_);
  completed_tasks_.clear();
  return true;
}

void CategorizedWorkerPoolImpl::Run(
    const std::vector<TaskCategory>& categories,
    base::ConditionVariable* has_ready_to_run_tasks_cv) {
  base::AutoLock lock(lock_);

  while (true) {
    if (!RunTaskWithLockAcquired(categories)) {
      // We are no longer running tasks, which may allow another category to
      // start running. Signal other worker threads.
      SignalHasReadyToRunTasksWithLockAcquired();

      // Exit when shutdown is set and no more tasks are pending.
      if (shutdown_) {
        break;
      }

      // Wait for more tasks.
      has_ready_to_run_tasks_cv->Wait();
      continue;
    }
  }
}

void CategorizedWorkerPoolImpl::FlushForTesting() {
  base::AutoLock lock(lock_);

  while (!work_queue_.HasFinishedRunningTasksInAllNamespaces()) {
    has_namespaces_with_finished_running_tasks_cv_.Wait();
  }
}

void CategorizedWorkerPoolImpl::ScheduleTasks(NamespaceToken token,
                                              TaskGraph* graph) {
  TRACE_EVENT2("disabled-by-default-cc.debug",
               "CategorizedWorkerPool::ScheduleTasks", "num_nodes",
               graph->nodes.size(), "num_edges", graph->edges.size());
  {
    base::AutoLock lock(lock_);
    ScheduleTasksWithLockAcquired(token, graph);
  }
}

void CategorizedWorkerPoolImpl::ScheduleTasksWithLockAcquired(
    NamespaceToken token,
    TaskGraph* graph) {
  DCHECK(token.IsValid());
  DCHECK(!TaskGraphWorkQueue::DependencyMismatch(graph));
  DCHECK(!shutdown_);

  work_queue_.ScheduleTasks(token, graph);

  // There may be more work available, so wake up another worker thread.
  SignalHasReadyToRunTasksWithLockAcquired();
}

bool CategorizedWorkerPoolImpl::RunTaskWithLockAcquired(
    const std::vector<TaskCategory>& categories) {
  for (const auto& category : categories) {
    if (ShouldRunTaskForCategoryWithLockAcquired(category)) {
      RunTaskInCategoryWithLockAcquired(category);
      return true;
    }
  }
  return false;
}

void CategorizedWorkerPoolImpl::RunTaskInCategoryWithLockAcquired(
    TaskCategory category) {
  lock_.AssertAcquired();

  auto prioritized_task = work_queue_.GetNextTaskToRun(category);

  TRACE_EVENT(
      "toplevel", "TaskGraphRunner::RunTask",
      perfetto::Flow::Global(prioritized_task.task->trace_task_id()),
      [&](perfetto::EventContext ctx) {
        ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
            ->set_chrome_raster_task()
            ->set_source_frame_number(prioritized_task.task->frame_number());
      });

  // There may be more work available, so wake up another worker thread.
  SignalHasReadyToRunTasksWithLockAcquired();

  {
    base::AutoUnlock unlock(lock_);
    prioritized_task.task->RunOnWorkerThread();
  }

  auto* task_namespace = prioritized_task.task_namespace.get();
  work_queue_.CompleteTask(std::move(prioritized_task));

  // If namespace has finished running all tasks, wake up origin threads.
  if (work_queue_.HasFinishedRunningTasksInNamespace(task_namespace)) {
    has_namespaces_with_finished_running_tasks_cv_.Signal();
  }
}

void CategorizedWorkerPoolImpl::SignalHasReadyToRunTasksWithLockAcquired() {
  lock_.AssertAcquired();

  for (TaskCategory category : kNormalThreadPriorityCategories) {
    if (ShouldRunTaskForCategoryWithLockAcquired(category)) {
      has_task_for_normal_priority_thread_cv_.Signal();
      return;
    }
  }

  // Due to the early return in the previous loop, this only runs when there are
  // no tasks to run on normal priority threads.
  for (TaskCategory category : kBackgroundThreadPriorityCategories) {
    if (ShouldRunTaskForCategoryWithLockAcquired(category)) {
      has_task_for_background_priority_thread_cv_.Signal();
      return;
    }
  }
}

CategorizedWorkerPoolJob::CategorizedWorkerPoolJob() = default;
CategorizedWorkerPoolJob::~CategorizedWorkerPoolJob() = default;

void CategorizedWorkerPoolJob::Start(int max_concurrency_foreground) {
  max_concurrency_foreground_ = max_concurrency_foreground;
  background_job_handle_ = base::CreateJob(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT, base::ThreadPolicy::PREFER_BACKGROUND,
       base::MayBlock()},
      base::BindRepeating(
          &CategorizedWorkerPoolJob::Run, base::Unretained(this),
          base::span<const TaskCategory>(kBackgroundThreadPriorityCategories)),
      base::BindRepeating(
          [](CategorizedWorkerPoolJob* self, size_t) {
            return std::min<size_t>(1U,
                                    self->GetMaxJobConcurrency(
                                        kBackgroundThreadPriorityCategories));
          },
          base::Unretained(this)));
  foreground_job_handle_ = base::CreateJob(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindRepeating(
          &CategorizedWorkerPoolJob::Run, base::Unretained(this),
          base::span<const TaskCategory>(kNormalThreadPriorityCategories)),
      base::BindRepeating(
          [](CategorizedWorkerPoolJob* self, size_t) {
            return std::min(
                self->max_concurrency_foreground_,
                self->GetMaxJobConcurrency(kNormalThreadPriorityCategories));
          },
          base::Unretained(this)));
}

void CategorizedWorkerPoolJob::Shutdown() {
  {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow;
    WaitForTasksToFinishRunning(namespace_token_);
  }

  CollectCompletedTasks(namespace_token_, &completed_tasks_);
  // Shutdown raster threads.
  {
    base::AutoLock lock(lock_);

    DCHECK(!work_queue_.HasReadyToRunTasks());
    DCHECK(!work_queue_.HasAnyNamespaces());
  }
  if (foreground_job_handle_) {
    foreground_job_handle_.Cancel();
  }
  if (background_job_handle_) {
    background_job_handle_.Cancel();
  }
}

// Overridden from base::TaskRunner:
bool CategorizedWorkerPoolJob::PostDelayedTask(const base::Location& from_here,
                                               base::OnceClosure task,
                                               base::TimeDelta delay) {
  base::JobHandle* job_handle_to_notify = nullptr;
  {
    base::AutoLock lock(lock_);

    // Remove completed tasks.
    DCHECK(completed_tasks_.empty());
    CollectCompletedTasksWithLockAcquired(namespace_token_, &completed_tasks_);

    std::erase_if(tasks_,
                  [this](const scoped_refptr<Task>& e)
                      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
                        return base::Contains(this->completed_tasks_, e);
                      });

    tasks_.push_back(base::MakeRefCounted<ClosureTask>(std::move(task)));
    graph_.Reset();
    for (const auto& graph_task : tasks_) {
      // Delayed tasks are assigned FOREGROUND category, ensuring that they run
      // as soon as possible once their delay has expired.
      graph_.nodes.push_back(
          TaskGraph::Node(graph_task.get(), TASK_CATEGORY_FOREGROUND,
                          0u /* priority */, 0u /* dependencies */));
    }

    job_handle_to_notify =
        ScheduleTasksWithLockAcquired(namespace_token_, &graph_);
    completed_tasks_.clear();
  }
  if (job_handle_to_notify) {
    job_handle_to_notify->NotifyConcurrencyIncrease();
  }
  return true;
}

void CategorizedWorkerPoolJob::Run(base::span<const TaskCategory> categories,
                                   base::JobDelegate* job_delegate) {
  std::optional<TaskGraphWorkQueue::PrioritizedTask> prioritized_task;

  while (!job_delegate->ShouldYield()) {
    base::JobHandle* job_handle_to_notify = nullptr;
    {
      base::AutoLock lock(lock_);
      // Pop a task for |categories|.
      prioritized_task = GetNextTaskToRunWithLockAcquired(categories);
      if (!prioritized_task) {
        // We are no longer running tasks, which may allow another category to
        // start running. Notify other worker jobs outside of |lock| below.
        job_handle_to_notify =
            ScheduleTasksWithLockAcquired(namespace_token_, &graph_);
      }
    }
    if (job_handle_to_notify) {
      job_handle_to_notify->NotifyConcurrencyIncrease();
    }

    // There's no pending task to run, quit the worker until notified again.
    if (!prioritized_task) {
      return;
    }
    TRACE_EVENT(
        "toplevel", "TaskGraphRunner::RunTask",
        [&](perfetto::EventContext ctx) {
          ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
              ->set_chrome_raster_task()
              ->set_source_frame_number(prioritized_task->task->frame_number());
        });

    base::ScopedAllowBaseSyncPrimitives allow;
    prioritized_task->task->RunOnWorkerThread();

    {
      base::AutoLock lock(lock_);

      auto* task_namespace = prioritized_task->task_namespace.get();
      work_queue_.CompleteTask(std::move(*prioritized_task));

      // If namespace has finished running all tasks, wake up origin threads.
      if (work_queue_.HasFinishedRunningTasksInNamespace(task_namespace)) {
        has_namespaces_with_finished_running_tasks_cv_.Signal();
      }
    }
  }
}

std::optional<TaskGraphWorkQueue::PrioritizedTask>
CategorizedWorkerPoolJob::GetNextTaskToRunWithLockAcquired(
    base::span<const TaskCategory> categories) {
  lock_.AssertAcquired();
  for (const auto& category : categories) {
    if (ShouldRunTaskForCategoryWithLockAcquired(category)) {
      return work_queue_.GetNextTaskToRun(category);
    }
  }
  return std::nullopt;
}

void CategorizedWorkerPoolJob::FlushForTesting() {
  foreground_job_handle_.Join();
  background_job_handle_.Join();
}

void CategorizedWorkerPoolJob::ScheduleTasks(NamespaceToken token,
                                             TaskGraph* graph) {
  TRACE_EVENT2("disabled-by-default-cc.debug",
               "CategorizedWorkerPool::ScheduleTasks", "num_nodes",
               graph->nodes.size(), "num_edges", graph->edges.size());
  base::JobHandle* job_handle_to_notify = nullptr;
  {
    base::AutoLock lock(lock_);
    job_handle_to_notify = ScheduleTasksWithLockAcquired(token, graph);
  }
  if (job_handle_to_notify) {
    job_handle_to_notify->NotifyConcurrencyIncrease();
  }
}

base::JobHandle* CategorizedWorkerPoolJob::ScheduleTasksWithLockAcquired(
    NamespaceToken token,
    TaskGraph* graph) {
  DCHECK(token.IsValid());
  DCHECK(!TaskGraphWorkQueue::DependencyMismatch(graph));

  work_queue_.ScheduleTasks(token, graph);
  return GetJobHandleToNotifyWithLockAcquired();
}

base::JobHandle*
CategorizedWorkerPoolJob::GetJobHandleToNotifyWithLockAcquired() {
  lock_.AssertAcquired();

  for (TaskCategory category : kNormalThreadPriorityCategories) {
    if (ShouldRunTaskForCategoryWithLockAcquired(category)) {
      return &foreground_job_handle_;
    }
  }

  // Due to the early return in the previous loop, this only runs when there are
  // no tasks to run on normal priority threads.
  for (TaskCategory category : kBackgroundThreadPriorityCategories) {
    if (ShouldRunTaskForCategoryWithLockAcquired(category)) {
      return &background_job_handle_;
    }
  }
  return nullptr;
}

size_t CategorizedWorkerPoolJob::GetMaxJobConcurrency(
    base::span<const TaskCategory> categories) const {
  base::AutoLock lock(lock_);

  bool has_foreground_tasks = false;
  for (TaskCategory foreground_category : kForegroundCategories) {
    if (work_queue_.NumRunningTasksForCategory(foreground_category) > 0 ||
        work_queue_.HasReadyToRunTasksForCategory(foreground_category)) {
      has_foreground_tasks = true;
      break;
    }
  }

  bool has_running_background_tasks = false;
  for (TaskCategory background_category : kBackgroundCategories) {
    has_running_background_tasks |=
        work_queue_.NumRunningTasksForCategory(background_category);
  }

  size_t num_foreground_tasks = 0;
  size_t num_background_tasks = 0;
  for (TaskCategory category : categories) {
    if (base::Contains(kBackgroundCategories, category)) {
      if (work_queue_.NumRunningTasksForCategory(category) > 0) {
        num_background_tasks = 1;
      }
      // Enforce that only one background task is allowed to run at a time, and
      // only if there are no foreground tasks running or ready to run.
      if (!has_running_background_tasks && !has_foreground_tasks &&
          work_queue_.HasReadyToRunTasksForCategory(category)) {
        num_background_tasks = 1;
      }
    } else if (category == TASK_CATEGORY_NONCONCURRENT_FOREGROUND) {
      // Enforce that only one nonconcurrent task is allowed to run at a time.
      if (work_queue_.NumRunningTasksForCategory(category) > 0 ||
          work_queue_.HasReadyToRunTasksForCategory(category)) {
        ++num_foreground_tasks;
      }
    } else {
      num_foreground_tasks += work_queue_.NumRunningTasksForCategory(category) +
                              work_queue_.NumReadyTasksForCategory(category);
    }
  }
  return num_foreground_tasks + num_background_tasks;
}

CategorizedWorkerPool* CategorizedWorkerPool::GetOrCreate(Delegate* delegate) {
  if (GetWorkerPool()) {
    return GetWorkerPool().get();
  }

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  int num_raster_threads = 1;
  if (command_line.HasSwitch(switches::kNumRasterThreads)) {
    std::string string_value =
        command_line.GetSwitchValueASCII(switches::kNumRasterThreads);
    bool parsed_num_raster_threads =
        base::StringToInt(string_value, &num_raster_threads);
    CHECK(parsed_num_raster_threads) << string_value;
    CHECK_GT(num_raster_threads, 0);
  }

  scoped_refptr<CategorizedWorkerPool> categorized_worker_pool =
      base::FeatureList::IsEnabled(kUseCompositorJob)
          ? scoped_refptr<CategorizedWorkerPool>(new CategorizedWorkerPoolJob())
          : scoped_refptr<CategorizedWorkerPool>(
                new CategorizedWorkerPoolImpl(delegate));
  categorized_worker_pool->Start(num_raster_threads);
  GetWorkerPool() = std::move(categorized_worker_pool);
  return GetWorkerPool().get();
}

CategorizedWorkerPool::CategorizedWorkerPool()
    : namespace_token_(GenerateNamespaceToken()),
      has_namespaces_with_finished_running_tasks_cv_(&lock_) {}

scoped_refptr<base::SequencedTaskRunner>
CategorizedWorkerPool::CreateSequencedTaskRunner() {
  return new CategorizedWorkerPoolSequencedTaskRunner(this);
}

CategorizedWorkerPool::~CategorizedWorkerPool() = default;

NamespaceToken CategorizedWorkerPool::GenerateNamespaceToken() {
  base::AutoLock lock(lock_);
  return work_queue_.GenerateNamespaceToken();
}

void CategorizedWorkerPool::WaitForTasksToFinishRunning(NamespaceToken token) {
  TRACE_EVENT0("disabled-by-default-cc.debug",
               "CategorizedWorkerPool::WaitForTasksToFinishRunning");

  DCHECK(token.IsValid());

  {
    base::AutoLock lock(lock_);

    auto* task_namespace = work_queue_.GetNamespaceForToken(token);

    if (!task_namespace) {
      return;
    }

    while (!work_queue_.HasFinishedRunningTasksInNamespace(task_namespace)) {
      has_namespaces_with_finished_running_tasks_cv_.Wait();
    }

    // There may be other namespaces that have finished running tasks, so wake
    // up another origin thread.
    has_namespaces_with_finished_running_tasks_cv_.Signal();
  }
}

void CategorizedWorkerPool::CollectCompletedTasks(
    NamespaceToken token,
    Task::Vector* completed_tasks) {
  TRACE_EVENT0("disabled-by-default-cc.debug",
               "CategorizedWorkerPool::CollectCompletedTasks");

  {
    base::AutoLock lock(lock_);
    CollectCompletedTasksWithLockAcquired(token, completed_tasks);
  }
}

void CategorizedWorkerPool::CollectCompletedTasksWithLockAcquired(
    NamespaceToken token,
    Task::Vector* completed_tasks) {
  DCHECK(token.IsValid());
  work_queue_.CollectCompletedTasks(token, completed_tasks);
}

bool CategorizedWorkerPool::ShouldRunTaskForCategoryWithLockAcquired(
    TaskCategory category) {
  lock_.AssertAcquired();

  if (!work_queue_.HasReadyToRunTasksForCategory(category)) {
    return false;
  }

  if (base::Contains(kBackgroundCategories, category)) {
    // Only run background tasks if there are no foreground tasks running or
    // ready to run.
    for (TaskCategory foreground_category : kForegroundCategories) {
      if (work_queue_.NumRunningTasksForCategory(foreground_category) > 0 ||
          work_queue_.HasReadyToRunTasksForCategory(foreground_category)) {
        return false;
      }
    }

    // Enforce that only one background task runs at a time.
    for (TaskCategory background_category : kBackgroundCategories) {
      if (work_queue_.NumRunningTasksForCategory(background_category) > 0) {
        return false;
      }
    }
  }

  // Enforce that only one nonconcurrent task runs at a time.
  if (category == TASK_CATEGORY_NONCONCURRENT_FOREGROUND &&
      work_queue_.NumRunningTasksForCategory(
          TASK_CATEGORY_NONCONCURRENT_FOREGROUND) > 0) {
    return false;
  }

  return true;
}

CategorizedWorkerPool::ClosureTask::ClosureTask(base::OnceClosure closure)
    : closure_(std::move(closure)) {}

// Overridden from Task:
void CategorizedWorkerPool::ClosureTask::RunOnWorkerThread() {
  std::move(closure_).Run();
}

CategorizedWorkerPool::ClosureTask::~ClosureTask() {}

}  // namespace cc
