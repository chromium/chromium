// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tasks_provider.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "ash/api/tasks/tasks_controller.h"
#include "ash/api/tasks/tasks_delegate.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/system/focus_mode/focus_mode_retry_util.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "google_apis/common/api_error_codes.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The tasks UI has limited space, so we restrict to showing N tasks.
constexpr size_t kTasksToFetch = 5;

// In order to get these tasks, we first query the API for task lists. We then
// query the task lists until we have received at least N tasks. To reduce
// latency, we query up to `kListFetchBatchSize` task lists in parallel.
constexpr size_t kListFetchBatchSize = 8;

// Controls the amount of time we'll serve a cached version of the task list.
constexpr base::TimeDelta kCacheLifetime = base::Seconds(30);

// Used to sort tasks for the carousel.
struct TaskComparator {
  // Tasks are classified into these groups and within each group sorted by
  // their update time. Tasks that have been created by the user in the focus
  // mode UI appear first, followed by past due tasks and so on.
  enum class TaskGroupOrdering {
    kCreatedInSession,
    kPastDue,
    kDueSoon,
    kDueLater,
  };

  bool operator()(const FocusModeTask& lhs, const FocusModeTask& rhs) const {
    auto lhs_group = GetOrdering(lhs);
    auto rhs_group = GetOrdering(rhs);
    if (lhs_group != rhs_group) {
      return lhs_group < rhs_group;
    }

    return lhs.updated > rhs.updated;
  }

  TaskGroupOrdering GetOrdering(const FocusModeTask& entry) const {
    if (created_task_ids->contains(entry.task_id)) {
      return TaskGroupOrdering::kCreatedInSession;
    }

    auto remaining = entry.due.value_or(base::Time::Max()) - now;
    if (remaining < base::Hours(0)) {
      return TaskGroupOrdering::kPastDue;
    } else if (remaining < base::Hours(24)) {
      return TaskGroupOrdering::kDueSoon;
    }
    return TaskGroupOrdering::kDueLater;
  }

  base::Time now;
  raw_ref<base::flat_set<TaskId>> created_task_ids;
};

}  // namespace

bool TaskId::IsValid() const {
  return !pending && !id.empty() && !list_id.empty();
}

std::strong_ordering TaskId::operator<=>(const TaskId& other) const {
  if (pending && other.pending) {
    // Two pending ids are always equivalent.
    return std::strong_ordering::equivalent;
  }
  if (pending != other.pending) {
    // If pending does not match, use the ordering for bools.
    return pending <=> other.pending;
  }

  if (list_id < other.list_id || (list_id == other.list_id && id < other.id)) {
    return std::strong_ordering::less;
  }
  if (list_id > other.list_id || (list_id == other.list_id && id > other.id)) {
    return std::strong_ordering::greater;
  }
  return std::strong_ordering::equivalent;
}

FocusModeTask::FocusModeTask() = default;
FocusModeTask::~FocusModeTask() = default;
FocusModeTask::FocusModeTask(const FocusModeTask&) = default;
FocusModeTask::FocusModeTask(FocusModeTask&&) = default;
FocusModeTask& FocusModeTask::operator=(const FocusModeTask&) = default;
FocusModeTask& FocusModeTask::operator=(FocusModeTask&&) = default;

// Helper used to fetch tasks from the API. It starts by querying for task
// lists, and then queries tasks from each list.
class TaskFetcher {
 public:
  void Start(base::OnceClosure done) {
    done_ = std::move(done);
    GetTaskListsInternal();
  }

  std::string GetMostRecentlyUpdatedTaskList() const {
    return task_lists_.empty() ? "" : task_lists_[0].first;
  }

  std::vector<FocusModeTask> GetTasks() && { return std::move(tasks_); }

  bool error() const { return error_; }

 private:
  // Invokes API request to get the task lists. It may be retried for certain
  // HTTP errors.
  void GetTaskListsInternal() {
    if (api::TasksDelegate* delegate =
            api::TasksController::Get()->tasks_delegate()) {
      delegate->GetTaskLists(
          /*force_fetch=*/true,
          base::BindOnce(&TaskFetcher::OnGetTaskLists,
                         weak_factory_.GetWeakPtr(),
                         /*start_time=*/base::Time::Now()));
    }
  }

  void GetTasksInternal(const std::string& list_id,
                        base::RepeatingClosure barrier) {
    if (api::TasksDelegate* delegate =
            api::TasksController::Get()->tasks_delegate()) {
      delegate->GetTasks(
          list_id,
          /*force_fetch=*/true,
          base::BindOnce(&TaskFetcher::OnGetTasks, weak_factory_.GetWeakPtr(),
                         /*start_time=*/base::Time::Now(), list_id, barrier));
    }
  }

  void OnGetTaskLists(const base::Time start_time,
                      bool success,
                      std::optional<google_apis::ApiErrorCode> http_error,
                      const ui::ListModel<api::TaskList>* api_task_lists) {
    const std::string method = "Tasks.GetTaskLists";
    focus_mode_util::RecordHistogramForApiStatus(
        method, http_error.value_or(google_apis::ApiErrorCode::HTTP_SUCCESS));
    focus_mode_util::RecordHistogramForApiLatency(
        method, base::Time::Now() - start_time);

    // Handle HTTP errors and apply retires.
    if (http_error.has_value() &&
        http_error.value() != google_apis::HTTP_SUCCESS) {
      // Handle too many request error. Retry if needed.
      if (http_error == 429 &&
          get_task_lists_retry_state_.retry_index < kMaxRetryTooManyRequests) {
        get_task_lists_retry_state_.retry_index++;
        get_task_lists_retry_state_.timer.Start(
            FROM_HERE, kWaitTimeTooManyRequests,
            base::BindOnce(&TaskFetcher::GetTaskListsInternal,
                           weak_factory_.GetWeakPtr()));
        return;
      }

      // Handle general HTTP errors. Retry if needed.
      if (ShouldRetryHttpError(http_error.value()) &&
          get_task_lists_retry_state_.retry_index < kMaxRetryOverall) {
        get_task_lists_retry_state_.retry_index++;
        get_task_lists_retry_state_.timer.Start(
            FROM_HERE,
            GetExponentialBackoffRetryWaitTime(
                get_task_lists_retry_state_.retry_index),
            base::BindOnce(&TaskFetcher::GetTaskListsInternal,
                           weak_factory_.GetWeakPtr()));
        return;
      }

      // Other unhandled HTTP errors or maximum retry reached. Bail gracefully.
      focus_mode_util::RecordHistogramForApiResult(method,
                                                   /*successful=*/false);
      focus_mode_util::RecordHistogramForApiRetryCount(
          method, get_task_lists_retry_state_.retry_index);
      error_ = true;
      get_task_lists_retry_state_.Reset();
      std::move(done_).Run();
      return;
    }

    focus_mode_util::RecordHistogramForApiRetryCount(
        method, get_task_lists_retry_state_.retry_index);

    if (!api_task_lists || api_task_lists->item_count() == 0) {
      focus_mode_util::RecordHistogramForApiResult(method,
                                                   /*successful=*/false);
      get_task_lists_retry_state_.Reset();
      std::move(done_).Run();
      return;
    }

    focus_mode_util::RecordHistogramForApiResult(method,
                                                 /*successful=*/true);

    // Collect the task lists and sort them so that the greatest one is first.
    task_lists_.reserve(api_task_lists->item_count());
    for (const auto& list : *api_task_lists) {
      task_lists_.emplace_back(list->id, list->updated);
    }
    base::ranges::sort(task_lists_, std::greater{},
                       &std::pair<std::string, base::Time>::second);

    MaybeFetchMoreTasks();
  }

  // If we haven't yet fetched enough tasks to show *and* there are lists that
  // haven't yet been queried, then try to fetch more tasks. In any other case,
  // we invoke the done callback.
  void MaybeFetchMoreTasks() {
    const auto lists_left = task_lists_.size() - task_list_fetch_index_;
    if (lists_left == 0 || tasks_.size() >= kTasksToFetch) {
      // We are done.
      std::move(done_).Run();
      return;
    }

    const auto batch_size = std::min(lists_left, kListFetchBatchSize);
    auto barrier = base::BarrierClosure(
        batch_size, base::BindOnce(&TaskFetcher::MaybeFetchMoreTasks,
                                   weak_factory_.GetWeakPtr()));

    // The code here is structured so that we don't modify any members after
    // calling `GetTasks`. This is done so that the code still works if
    // `GetTasks` invokes the callback synchronously (which happens in tests).
    auto next_task_list_index = task_list_fetch_index_;
    task_list_fetch_index_ += batch_size;

    for (size_t i = 0; i != batch_size; ++i) {
      const std::string& list_id = task_lists_[next_task_list_index++].first;
      GetTasksInternal(list_id, barrier);
    }
  }

  void OnGetTasks(const base::Time start_time,
                  const std::string& list_id,
                  base::RepeatingClosure barrier,
                  bool success,
                  std::optional<google_apis::ApiErrorCode> http_error,
                  const ui::ListModel<api::Task>* api_tasks) {
    const std::string method = "Tasks.GetTasks";
    focus_mode_util::RecordHistogramForApiStatus(
        method, http_error.value_or(google_apis::ApiErrorCode::HTTP_SUCCESS));
    focus_mode_util::RecordHistogramForApiLatency(
        method, base::Time::Now() - start_time);

    // Handle HTTP errors and apply retires.
    if (http_error.has_value() &&
        http_error.value() != google_apis::HTTP_SUCCESS) {
      // Handle too many request error. Retry if needed.
      if (http_error == 429 &&
          get_tasks_retry_state_.retry_index < kMaxRetryTooManyRequests) {
        get_tasks_retry_state_.retry_index++;
        get_tasks_retry_state_.timer.Start(
            FROM_HERE, kWaitTimeTooManyRequests,
            base::BindOnce(&TaskFetcher::GetTasksInternal,
                           weak_factory_.GetWeakPtr(), list_id, barrier));
        return;
      }

      // Handle general HTTP errors. Retry if needed.
      if (ShouldRetryHttpError(http_error.value()) &&
          get_tasks_retry_state_.retry_index < kMaxRetryOverall) {
        get_tasks_retry_state_.retry_index++;
        get_tasks_retry_state_.timer.Start(
            FROM_HERE,
            GetExponentialBackoffRetryWaitTime(
                get_tasks_retry_state_.retry_index),
            base::BindOnce(&TaskFetcher::GetTasksInternal,
                           weak_factory_.GetWeakPtr(), list_id, barrier));
        return;
      }

      // Other unhandled HTTP errors or maximum retry reached. Bail gracefully.
      focus_mode_util::RecordHistogramForApiRetryCount(
          method, get_tasks_retry_state_.retry_index);
      focus_mode_util::RecordHistogramForApiResult(method,
                                                   /*successful=*/false);
      get_tasks_retry_state_.Reset();
      std::move(barrier).Run();
      return;
    }

    focus_mode_util::RecordHistogramForApiRetryCount(
        method, get_tasks_retry_state_.retry_index);
    focus_mode_util::RecordHistogramForApiResult(
        method,
        /*successful=*/success && api_tasks);

    // NOTE: Completed tasks will not show up in `api_tasks`.
    if (success && api_tasks) {
      for (const auto& api_task : *api_tasks) {
        // Skip tasks with empty titles.
        if (api_task->title.empty()) {
          continue;
        }
        FocusModeTask& task = tasks_.emplace_back();
        task.task_id = {.list_id = list_id, .id = api_task->id};
        task.title = api_task->title;
        task.updated = api_task->updated;
        task.due = api_task->due;
      }
    }

    // Do not do anything with `this` after this line since the fetcher will be
    // deleted after the last list has been queried.
    std::move(barrier).Run();
  }

  // This will only be set after retries if retries are conducted.
  bool error_ = false;

  // Task list IDs, sorted by creation time.
  std::vector<std::pair<std::string, base::Time>> task_lists_;

  // The index of the next task list to fetch tasks for.
  std::size_t task_list_fetch_index_ = 0;

  // Tasks fetched.
  std::vector<FocusModeTask> tasks_;

  // Invoked when the fetcher is complete.
  base::OnceClosure done_;

  FocusModeRetryState get_task_lists_retry_state_;
  FocusModeRetryState get_tasks_retry_state_;

  base::WeakPtrFactory<TaskFetcher> weak_factory_{this};
};

FocusModeTasksProvider::FocusModeTasksProvider() = default;
FocusModeTasksProvider::~FocusModeTasksProvider() = default;

void FocusModeTasksProvider::ScheduleTaskListUpdate() {
  if (!task_fetcher_) {
    // We don't start a new fetch if a fetch is already running.
    task_fetcher_ = std::make_unique<TaskFetcher>();
    task_fetcher_->Start(base::BindOnce(&FocusModeTasksProvider::OnTasksFetched,
                                        weak_factory_.GetWeakPtr()));
  }
}

void FocusModeTasksProvider::Reset() {
  task_fetcher_ = nullptr;
  task_fetch_time_ = {};
  task_list_for_new_task_ = {};
  tasks_.clear();
  deleted_task_ids_.clear();
}

const std::vector<FocusModeTask> FocusModeTasksProvider::TasksForTesting()
    const {
  return tasks_;
}

void FocusModeTasksProvider::GetSortedTaskList(OnGetTasksCallback callback) {
  if ((base::Time::Now() - task_fetch_time_) < kCacheLifetime) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetSortedTasksImpl()));
    return;
  }

  get_tasks_requests_.push_back(std::move(callback));
  ScheduleTaskListUpdate();
}

void FocusModeTasksProvider::GetTask(const std::string& task_list_id,
                                     const std::string& task_id,
                                     OnGetTaskCallback callback) {
  CHECK(!task_list_id.empty());
  CHECK(!task_id.empty());

  if (api::TasksDelegate* delegate =
          api::TasksController::Get()->tasks_delegate()) {
    delegate->GetTasks(
        task_list_id, /*force_fetch=*/true,
        base::BindOnce(&FocusModeTasksProvider::OnTasksFetchedForTask,
                       weak_factory_.GetWeakPtr(),
                       /*start_time=*/base::Time::Now(), task_list_id, task_id,
                       std::move(callback)));
  }
}

void FocusModeTasksProvider::AddTask(const std::string& title,
                                     OnTaskSavedCallback callback) {
  if (task_list_for_new_task_.empty()) {
    // TODO(b/339667327): Instead of failing the request, consider queueing it.
    std::move(callback).Run(FocusModeTask{});
    return;
  }

  // Clear the cache. This is done so that the backend is queried the next time
  // a task list is requested. This in turn is done so that we can get the
  // actual ID of the newly created task.
  task_fetch_time_ = {};
  AddTaskInternal(title, std::move(callback));
}

void FocusModeTasksProvider::UpdateTask(const std::string& task_list_id,
                                        const std::string& task_id,
                                        const std::string& title,
                                        bool completed,
                                        OnTaskSavedCallback callback) {
  CHECK(!task_id.empty());
  CHECK(!task_list_id.empty());

  if (completed) {
    deleted_task_ids_.insert({.list_id = task_list_id, .id = task_id});
  }

  UpdateTaskInternal(task_list_id, task_id, title, completed,
                     std::move(callback));
}

void FocusModeTasksProvider::OnTasksFetched() {
  CHECK(task_fetcher_);

  if (!task_fetcher_->error()) {
    task_fetch_time_ = base::Time::Now();
    task_list_for_new_task_ = task_fetcher_->GetMostRecentlyUpdatedTaskList();
    tasks_ = std::move(*task_fetcher_).GetTasks();
  } else {
    tasks_ = {};
    task_list_for_new_task_ = {};
  }
  task_fetcher_ = nullptr;

  // Make sure to clear this in case there are tasks completed through Focus
  // mode that the user then un-completed outside of Focus mode.
  deleted_task_ids_ = {};

  auto pending = std::move(get_tasks_requests_);
  auto tasks = GetSortedTasksImpl();
  for (auto& callback : pending) {
    std::move(callback).Run(tasks);
  }
}

void FocusModeTasksProvider::OnTasksFetchedForTask(
    const base::Time start_time,
    const std::string& task_list_id,
    const std::string& task_id,
    OnGetTaskCallback callback,
    bool success,
    std::optional<google_apis::ApiErrorCode> http_error,
    const ui::ListModel<api::Task>* api_tasks) {
  const std::string method = "Tasks.GetTasks";
  focus_mode_util::RecordHistogramForApiStatus(
      method, http_error.value_or(google_apis::ApiErrorCode::HTTP_SUCCESS));
  focus_mode_util::RecordHistogramForApiLatency(method,
                                                base::Time::Now() - start_time);

  // Handle HTTP errors and apply retires.
  if (http_error.has_value() &&
      http_error.value() != google_apis::HTTP_SUCCESS) {
    // Handle too many request error. Retry if needed.
    if (http_error == 429 &&
        get_task_retry_state_.retry_index < kMaxRetryTooManyRequests) {
      get_task_retry_state_.retry_index++;
      get_task_retry_state_.timer.Start(
          FROM_HERE, kWaitTimeTooManyRequests,
          base::BindOnce(&FocusModeTasksProvider::GetTask,
                         weak_factory_.GetWeakPtr(), task_list_id, task_id,
                         std::move(callback)));
      return;
    }

    // Handle general HTTP errors. Retry if needed.
    if (ShouldRetryHttpError(http_error.value()) &&
        get_task_retry_state_.retry_index < kMaxRetryOverall) {
      get_task_retry_state_.retry_index++;
      get_task_retry_state_.timer.Start(
          FROM_HERE,
          GetExponentialBackoffRetryWaitTime(get_task_retry_state_.retry_index),
          base::BindOnce(&FocusModeTasksProvider::GetTask,
                         weak_factory_.GetWeakPtr(), task_list_id, task_id,
                         std::move(callback)));
      return;
    }

    // Other unhandled HTTP errors or maximum retry reached. Bail gracefully.
    focus_mode_util::RecordHistogramForApiRetryCount(
        method, get_task_retry_state_.retry_index);
    focus_mode_util::RecordHistogramForApiResult(method,
                                                 /*successful=*/false);
    std::move(callback).Run(FocusModeTask{});
    get_task_retry_state_.Reset();
    return;
  }

  focus_mode_util::RecordHistogramForApiRetryCount(
      method, get_task_retry_state_.retry_index);
  focus_mode_util::RecordHistogramForApiResult(method,
                                               /*successful=*/success);

  if (!success) {
    std::move(callback).Run(FocusModeTask{});
    get_task_retry_state_.Reset();
    return;
  }

  TaskId fetched_task_id = {.list_id = task_list_id, .id = task_id};
  auto iter =
      base::ranges::find(tasks_, fetched_task_id, &FocusModeTask::task_id);
  bool task_exists = iter != tasks_.end();

  FocusModeTask temp_local_task;
  if (!task_exists) {
    temp_local_task.task_id = fetched_task_id;
  }
  FocusModeTask& task = task_exists ? *iter : temp_local_task;

  // Make sure that the fetched task is updated in the cache if it exists.
  // NOTE: Completed tasks will not show up in `api_tasks`, so we first assume
  // it's completed and update the state if the task is found in `api_tasks`.
  // TODO: Can we actually verify that the task is complete instead of making
  // this assumption?
  task.completed = true;

  for (const auto& api_task : *api_tasks) {
    if (api_task->id == task_id) {
      task.title = api_task->title;
      task.updated = api_task->updated;
      task.completed = api_task->completed;
      break;
    }
  }
  if (task.completed && task_exists) {
    // Only mark the task as deleted if it already exists in `tasks_`.
    deleted_task_ids_.insert(fetched_task_id);
  }

  std::move(callback).Run(task);
  get_task_retry_state_.Reset();
}

void FocusModeTasksProvider::OnTaskAdded(const base::Time start_time,
                                         const std::string& title,
                                         OnTaskSavedCallback callback,
                                         google_apis::ApiErrorCode http_error,
                                         const api::Task* api_task) {
  const std::string method = "Tasks.AddTask";
  focus_mode_util::RecordHistogramForApiStatus(method, http_error);
  focus_mode_util::RecordHistogramForApiLatency(method,
                                                base::Time::Now() - start_time);

  if (!api_task || api_task->title.empty()) {
    // When `api_task` is null, `http_error` can be
    // `google_apis::ApiErrorCode::HTTP_SUCCESS` or other error code. Retry some
    // of the error codes as well.
    if (http_error != google_apis::ApiErrorCode::HTTP_SUCCESS) {
      // Handle too many requests error.
      if (http_error == 429 &&
          add_task_retry_state_.retry_index < kMaxRetryTooManyRequests) {
        // Retry if needed.
        add_task_retry_state_.retry_index++;
        add_task_retry_state_.timer.Start(
            FROM_HERE, kWaitTimeTooManyRequests,
            base::BindOnce(&FocusModeTasksProvider::AddTaskInternal,
                           weak_factory_.GetWeakPtr(), title,
                           std::move(callback)));
        return;
      }

      // Handle general HTTP errors.
      if (ShouldRetryHttpError(http_error) &&
          add_task_retry_state_.retry_index < kMaxRetryOverall) {
        // Retry if needed.
        add_task_retry_state_.retry_index++;
        add_task_retry_state_.timer.Start(
            FROM_HERE,
            GetExponentialBackoffRetryWaitTime(
                add_task_retry_state_.retry_index),
            base::BindOnce(&FocusModeTasksProvider::AddTaskInternal,
                           weak_factory_.GetWeakPtr(), title,
                           std::move(callback)));
        return;
      }
    }

    // After all of the retries, if there's still an error, we clear the cache.
    focus_mode_util::RecordHistogramForApiRetryCount(
        method, add_task_retry_state_.retry_index);
    focus_mode_util::RecordHistogramForApiResult(method,
                                                 /*successful=*/false);
    task_fetch_time_ = {};
    std::move(callback).Run(FocusModeTask{});
    add_task_retry_state_.Reset();
    return;
  }

  focus_mode_util::RecordHistogramForApiRetryCount(
      method, add_task_retry_state_.retry_index);
  focus_mode_util::RecordHistogramForApiResult(method,
                                               /*successful=*/true);

  UpdateOrInsertTask(task_list_for_new_task_, api_task, std::move(callback));
  add_task_retry_state_.Reset();
}

void FocusModeTasksProvider::OnTaskUpdated(const base::Time start_time,
                                           const std::string& task_list_id,
                                           const std::string& task_id,
                                           const std::string& title,
                                           bool completed,
                                           OnTaskSavedCallback callback,
                                           google_apis::ApiErrorCode http_error,
                                           const api::Task* api_task) {
  const std::string method = "Tasks.UpdateTask";
  focus_mode_util::RecordHistogramForApiStatus(method, http_error);
  focus_mode_util::RecordHistogramForApiLatency(method,
                                                base::Time::Now() - start_time);

  if (!api_task || api_task->title.empty()) {
    // When `api_task` is null, `http_error` can be
    // `google_apis::ApiErrorCode::HTTP_SUCCESS` or other error code. Retry some
    // of the error codes as well.
    if (http_error != google_apis::ApiErrorCode::HTTP_SUCCESS) {
      // Handle too many requests error. Retry if needed.
      if (http_error == 429 &&
          update_task_retry_state_.retry_index < kMaxRetryTooManyRequests) {
        update_task_retry_state_.retry_index++;
        update_task_retry_state_.timer.Start(
            FROM_HERE, kWaitTimeTooManyRequests,
            base::BindOnce(&FocusModeTasksProvider::UpdateTaskInternal,
                           weak_factory_.GetWeakPtr(), task_list_id, task_id,
                           title, completed, std::move(callback)));
        return;
      }

      // Handle general HTTP errors. Retry if needed.
      if (ShouldRetryHttpError(http_error) &&
          update_task_retry_state_.retry_index < kMaxRetryOverall) {
        update_task_retry_state_.retry_index++;
        update_task_retry_state_.timer.Start(
            FROM_HERE,
            GetExponentialBackoffRetryWaitTime(
                update_task_retry_state_.retry_index),
            base::BindOnce(&FocusModeTasksProvider::UpdateTaskInternal,
                           weak_factory_.GetWeakPtr(), task_list_id, task_id,
                           title, completed, std::move(callback)));
        return;
      }
    }

    // After all of the retries, if there's still an error, we clear the cache.
    focus_mode_util::RecordHistogramForApiRetryCount(
        method, update_task_retry_state_.retry_index);
    focus_mode_util::RecordHistogramForApiResult(method,
                                                 /*successful=*/false);
    task_fetch_time_ = {};
    if (completed) {
      deleted_task_ids_.erase({.list_id = task_list_id, .id = task_id});
    }
    std::move(callback).Run(FocusModeTask{});
    update_task_retry_state_.Reset();
    return;
  }

  focus_mode_util::RecordHistogramForApiRetryCount(
      method, update_task_retry_state_.retry_index);
  focus_mode_util::RecordHistogramForApiResult(method,
                                               /*successful=*/true);

  UpdateOrInsertTask(task_list_id, api_task, std::move(callback));
  update_task_retry_state_.Reset();
}

void FocusModeTasksProvider::AddTaskInternal(const std::string& title,
                                             OnTaskSavedCallback callback) {
  api::TasksController::Get()->tasks_delegate()->AddTask(
      task_list_for_new_task_, title,
      base::BindOnce(
          &FocusModeTasksProvider::OnTaskAdded, weak_factory_.GetWeakPtr(),
          /*start_time=*/base::Time::Now(), title, std::move(callback)));
}

void FocusModeTasksProvider::UpdateTaskInternal(const std::string& task_list_id,
                                                const std::string& task_id,
                                                const std::string& title,
                                                bool completed,
                                                OnTaskSavedCallback callback) {
  api::TasksController::Get()->tasks_delegate()->UpdateTask(
      task_list_id, task_id, title, completed,
      base::BindOnce(&FocusModeTasksProvider::OnTaskUpdated,
                     weak_factory_.GetWeakPtr(),
                     /*start_time=*/base::Time::Now(), task_list_id, task_id,
                     title, completed, std::move(callback)));
}

void FocusModeTasksProvider::UpdateOrInsertTask(const std::string& task_list_id,
                                                const api::Task* api_task,
                                                OnTaskSavedCallback callback) {
  TaskId created_id = {.list_id = task_list_id, .id = api_task->id};
  created_task_ids_.insert(created_id);

  // Try to find the task in the cache or insert it.
  auto iter = base::ranges::find(tasks_, created_id, &FocusModeTask::task_id);

  FocusModeTask& task = (iter != tasks_.end()) ? *iter : tasks_.emplace_back();
  task.task_id = created_id;
  task.title = api_task->title;
  task.updated = api_task->updated;

  std::move(callback).Run(task);
}

std::vector<FocusModeTask> FocusModeTasksProvider::GetSortedTasksImpl() {
  std::vector<FocusModeTask> result;
  for (const FocusModeTask& task : tasks_) {
    if (!deleted_task_ids_.contains(task.task_id)) {
      result.push_back(task);
    }
  }

  base::ranges::sort(
      result, TaskComparator{base::Time::Now(), raw_ref<base::flat_set<TaskId>>(
                                                    created_task_ids_)});

  return result;
}

}  // namespace ash
