// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/api/tasks/tasks_client_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/tasks/tasks_api_request_types.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "google_apis/tasks/tasks_api_response_types.h"
#include "google_apis/tasks/tasks_api_task_status.h"
#include "ui/base/models/list_model.h"

namespace ash::api {
namespace {

using ::google_apis::ApiErrorCode;
using ::google_apis::tasks::InsertTaskRequest;
using ::google_apis::tasks::ListTaskListsRequest;
using ::google_apis::tasks::ListTasksRequest;
using ::google_apis::tasks::PatchTaskRequest;
using ::google_apis::tasks::TaskAssignmentInfo;
using ::google_apis::tasks::TaskRequestPayload;
using ::google_apis::tasks::TaskStatus;

constexpr char kTasksUrl[] = "https://tasks.google.com/";

Task::OriginSurfaceType GetTaskOriginSurfaceType(
    const std::optional<TaskAssignmentInfo>& assignment_info) {
  if (!assignment_info) {
    return Task::OriginSurfaceType::kRegular;
  }

  switch (assignment_info->surface_type()) {
    case TaskAssignmentInfo::SurfaceType::kDocument:
      return Task::OriginSurfaceType::kDocument;
    case TaskAssignmentInfo::SurfaceType::kSpace:
      return Task::OriginSurfaceType::kSpace;
    case TaskAssignmentInfo::SurfaceType::kUnknown:
      return Task::OriginSurfaceType::kUnknown;
  }
}

// Converts `raw_tasks` received from Google Tasks API to ash-friendly types.
std::vector<std::unique_ptr<Task>> ConvertTasks(
    const std::vector<std::unique_ptr<google_apis::tasks::Task>>& raw_tasks) {
  // Find root level tasks and collect task ids that have subtasks in one pass.
  std::vector<const google_apis::tasks::Task*> root_tasks;
  base::flat_set<std::string> tasks_with_subtasks;
  for (const auto& item : raw_tasks) {
    if (item->parent_id().empty()) {
      root_tasks.push_back(item.get());
    } else {
      tasks_with_subtasks.insert(item->parent_id());
    }
  }

  // Sort tasks by their position as they appear in the companion app with "My
  // order" option selected.
  // NOTE: ideally sorting should be performed on the UI/presentation layer, but
  // there is a possibility that with further optimizations and plans to keep
  // only top N visible tasks in memory, the sorting will need to be done at
  // this layer.
  std::sort(
      root_tasks.begin(), root_tasks.end(),
      [](const google_apis::tasks::Task* a, const google_apis::tasks::Task* b) {
        return a->position().compare(b->position()) < 0;
      });

  // Convert `root_tasks` to ash-friendly types.
  std::vector<std::unique_ptr<Task>> converted_tasks;
  converted_tasks.reserve(root_tasks.size());
  for (const auto* const root_task : root_tasks) {
    const bool completed = root_task->status() == TaskStatus::kCompleted;
    const bool has_subtasks = tasks_with_subtasks.contains(root_task->id());
    const bool has_email_link =
        std::find_if(root_task->links().begin(), root_task->links().end(),
                     [](const auto& link) {
                       return link->type() ==
                              google_apis::tasks::TaskLink::Type::kEmail;
                     }) != root_task->links().end();
    const bool has_notes = !root_task->notes().empty();
    converted_tasks.push_back(std::make_unique<Task>(
        root_task->id(), root_task->title(), root_task->due(), completed,
        has_subtasks, has_email_link, has_notes, root_task->updated(),
        root_task->web_view_link(),
        GetTaskOriginSurfaceType(root_task->assignment_info())));
  }

  return converted_tasks;
}

}  // namespace

TasksClientImpl::TaskListsFetchState::TaskListsFetchState() = default;

TasksClientImpl::TaskListsFetchState::~TaskListsFetchState() = default;

TasksClientImpl::TasksFetchState::TasksFetchState() = default;

TasksClientImpl::TasksFetchState::~TasksFetchState() = default;

TasksClientImpl::TasksClientImpl(
    Profile* profile,
    const TasksClientImpl::CreateRequestSenderCallback&
        create_request_sender_callback,
    net::NetworkTrafficAnnotationTag traffic_annotation_tag)
    : profile_(profile),
      create_request_sender_callback_(create_request_sender_callback),
      traffic_annotation_tag_(traffic_annotation_tag) {}

TasksClientImpl::~TasksClientImpl() = default;

bool TasksClientImpl::IsDisabledByAdmin() const {
  // 1) Check the pref.
  const auto* const pref_service = profile_->GetPrefs();
  if (!pref_service ||
      !base::Contains(pref_service->GetList(
                          prefs::kContextualGoogleIntegrationsConfiguration),
                      prefs::kGoogleTasksIntegrationName)) {
    RecordContextualGoogleIntegrationStatus(
        prefs::kGoogleTasksIntegrationName,
        ContextualGoogleIntegrationStatus::kDisabledByPolicy);
    return true;
  }

  // 2) Check if the Calendar app (home app for Tasks) is disabled by policy.
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          profile_)) {
    return true;
  }
  auto calendar_app_readiness = apps::Readiness::kUnknown;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(web_app::kGoogleCalendarAppId,
                 [&calendar_app_readiness](const apps::AppUpdate& update) {
                   calendar_app_readiness = update.Readiness();
                 });
  if (calendar_app_readiness == apps::Readiness::kDisabledByPolicy) {
    RecordContextualGoogleIntegrationStatus(
        prefs::kGoogleTasksIntegrationName,
        ContextualGoogleIntegrationStatus::kDisabledByAppBlock);
    return true;
  }

  // 3) Check if the Tasks URL is blocked by policy.
  const auto* const policy_blocklist_service =
      PolicyBlocklistFactory::GetForBrowserContext(profile_);
  if (!policy_blocklist_service ||
      policy_blocklist_service->GetURLBlocklistState(GURL(kTasksUrl)) ==
          policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    RecordContextualGoogleIntegrationStatus(
        prefs::kGoogleTasksIntegrationName,
        ContextualGoogleIntegrationStatus::kDisabledByUrlBlock);
    return true;
  }

  RecordContextualGoogleIntegrationStatus(
      prefs::kGoogleTasksIntegrationName,
      ContextualGoogleIntegrationStatus::kEnabled);
  return false;
}

const ui::ListModel<api::TaskList>* TasksClientImpl::GetCachedTaskLists() {
  // Note that this doesn't consider `task_lists_fetch_state_`, which means that
  // the returned `task_lists_` may contain data that was fetched long time ago.
  if (task_lists_.item_count() == 0) {
    return nullptr;
  }

  return &task_lists_;
}

void TasksClientImpl::GetTaskLists(bool force_fetch,
                                   TasksClient::GetTaskListsCallback callback) {
  if (task_lists_fetch_state_.status == FetchStatus::kFresh && !force_fetch) {
    // The http error code should be null here since we use the cached fresh
    // data instead of fetching from the Google Task API.
    std::move(callback).Run(/*success=*/true, std::nullopt, &task_lists_);
    return;
  }

  task_lists_fetch_state_.callbacks.push_back(std::move(callback));

  if (task_lists_fetch_state_.status != FetchStatus::kRefreshing) {
    task_lists_fetch_state_.status = FetchStatus::kRefreshing;
    FetchTaskListsPage(/*page_token=*/"", /*page_number=*/1,
                       /*accumulated_raw_task_lists=*/{});
  }
}

const ui::ListModel<api::Task>* TasksClientImpl::GetCachedTasksInTaskList(
    const std::string& task_list_id) {
  // Note that this doesn't consider `tasks_fetch_state_`, which means that the
  // returned tasks may contain data that was fetched long time ago.
  const auto iter = tasks_in_task_lists_.find(task_list_id);
  if (iter == tasks_in_task_lists_.end()) {
    return nullptr;
  }

  return &iter->second;
}

void TasksClientImpl::GetTasks(const std::string& task_list_id,
                               bool force_fetch,
                               TasksClient::GetTasksCallback callback) {
  CHECK(!task_list_id.empty());

  // TODO(b/337281579): Creating a list model in `tasks_in_task_lists_` before
  // the tasks are actually returned will have `GetCachedTasksInTaskList()`
  // return an empty list model, which is wrong as it is not the actual result.
  // Move this to `RunGetTasksCallbacks()` will make more sense.
  const auto [iter, inserted] = tasks_in_task_lists_.emplace(
      std::piecewise_construct, std::forward_as_tuple(task_list_id),
      std::forward_as_tuple());

  const auto [status_it, state_inserted] =
      tasks_fetch_state_.emplace(task_list_id, nullptr);
  if (!status_it->second) {
    status_it->second = std::make_unique<TasksFetchState>();
  }
  TasksFetchState& fetch_state = *status_it->second;
  if (fetch_state.status == FetchStatus::kFresh && !force_fetch) {
    // The http error code should be null here since we use the cached fresh
    // data instead of fetching from the Google Task API.
    std::move(callback).Run(/*success=*/true, std::nullopt, &iter->second);
    return;
  }

  fetch_state.callbacks.push_back(std::move(callback));

  if (fetch_state.status != FetchStatus::kRefreshing) {
    fetch_state.status = FetchStatus::kRefreshing;
    FetchTasksPage(task_list_id, /*page_token=*/"", /*page_number=*/1,
                   /*accumulated_raw_tasks=*/{});
  }
}

void TasksClientImpl::MarkAsCompleted(const std::string& task_list_id,
                                      const std::string& task_id,
                                      bool completed) {
  CHECK(!task_list_id.empty());
  CHECK(!task_id.empty());

  if (completed) {
    pending_completed_tasks_[task_list_id].insert(task_id);
  } else {
    if (pending_completed_tasks_.contains(task_list_id)) {
      pending_completed_tasks_[task_list_id].erase(task_id);
    }
  }
}

void TasksClientImpl::AddTask(const std::string& task_list_id,
                              const std::string& title,
                              TasksClient::OnTaskSavedCallback callback) {
  CHECK(!task_list_id.empty());
  CHECK(!title.empty());
  CHECK(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(std::make_unique<InsertTaskRequest>(
      request_sender, task_list_id, /*previous_task_id=*/"",
      TaskRequestPayload{.title = title, .status = TaskStatus::kNeedsAction},
      base::BindOnce(&TasksClientImpl::OnTaskAdded, weak_factory_.GetWeakPtr(),
                     task_list_id, base::Time::Now(), std::move(callback))));
}

void TasksClientImpl::UpdateTask(const std::string& task_list_id,
                                 const std::string& task_id,
                                 const std::string& title,
                                 bool completed,
                                 TasksClient::OnTaskSavedCallback callback) {
  CHECK(!task_list_id.empty());
  CHECK(!task_id.empty());
  CHECK(!title.empty() || completed);
  CHECK(callback);

  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(std::make_unique<PatchTaskRequest>(
      request_sender, task_list_id, task_id,
      TaskRequestPayload{.title = title,
                         .status = completed ? TaskStatus::kCompleted
                                             : TaskStatus::kNeedsAction},
      base::BindOnce(&TasksClientImpl::OnTaskUpdated,
                     weak_factory_.GetWeakPtr(), task_list_id,
                     base::Time::Now(), std::move(callback))));
}

std::optional<base::Time> TasksClientImpl::GetTasksLastUpdateTime(
    const std::string& task_list_id) const {
  const auto iter = tasks_fetch_state_.find(task_list_id);
  if (iter == tasks_fetch_state_.end()) {
    return std::nullopt;
  }
  return iter->second->last_updated_time;
}

void TasksClientImpl::InvalidateCache() {
  for (auto& task_list_state : tasks_fetch_state_) {
    if (task_list_state.second->status == FetchStatus::kRefreshing) {
      RunGetTasksCallbacks(task_list_state.first, FetchStatus::kNotFresh,
                           std::nullopt,
                           /*accumulated_raw_tasks=*/{});
    } else {
      task_list_state.second->status = FetchStatus::kNotFresh;
    }
  }

  if (task_lists_fetch_state_.status == FetchStatus::kRefreshing) {
    RunGetTaskListsCallbacks(FetchStatus::kNotFresh, std::nullopt,
                             /*accumulated_raw_task_lists=*/{});
  } else {
    task_lists_fetch_state_.status = FetchStatus::kNotFresh;
  }
}

void TasksClientImpl::OnGlanceablesBubbleClosed(base::OnceClosure callback) {
  // TODO(b/324462272): We need to watch this. This could cause one client to
  // cancel the in-flight callbacks for requests sent by another client. This
  // could become an issue when we have multiple clients for one
  // `TasksClientImpl`. Remove this after adding a `kRefreshingInvalidated`
  // state.
  weak_factory_.InvalidateWeakPtrs();

  int num_tasks_completed = 0;
  for (const auto& [task_list_id, task_ids] : pending_completed_tasks_) {
    num_tasks_completed += task_ids.size();
  }
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(num_tasks_completed, std::move(callback));

  // TODO(b/323975767): Generalize this histogram to the Tasks API.
  base::UmaHistogramCounts100(
      "Ash.Glanceables.Api.Tasks.SimultaneousMarkAsCompletedRequestsCount",
      num_tasks_completed);

  for (const auto& [task_list_id, task_ids] : pending_completed_tasks_) {
    for (const auto& task_id : task_ids) {
      auto* const request_sender = GetRequestSender();
      request_sender->StartRequestWithAuthRetry(
          std::make_unique<PatchTaskRequest>(
              request_sender, task_list_id, task_id,
              TaskRequestPayload{.status = TaskStatus::kCompleted},
              base::BindOnce(&TasksClientImpl::OnMarkedAsCompleted,
                             weak_factory_.GetWeakPtr(), base::Time::Now(),
                             barrier_closure)));
    }
  }
  pending_completed_tasks_.clear();

  InvalidateCache();
}

void TasksClientImpl::FetchTaskListsPage(
    const std::string& page_token,
    int page_number,
    std::vector<std::unique_ptr<google_apis::tasks::TaskList>>
        accumulated_raw_task_lists) {
  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(
      std::make_unique<ListTaskListsRequest>(
          request_sender, page_token,
          base::BindOnce(&TasksClientImpl::OnTaskListsPageFetched,
                         weak_factory_.GetWeakPtr(), base::Time::Now(),
                         page_number, std::move(accumulated_raw_task_lists))));
  if (task_lists_request_callback_) {
    task_lists_request_callback_.Run(page_token);
  }
}

void TasksClientImpl::OnTaskListsPageFetched(
    const base::Time& request_start_time,
    int page_number,
    std::vector<std::unique_ptr<google_apis::tasks::TaskList>>
        accumulated_raw_task_lists,
    base::expected<std::unique_ptr<google_apis::tasks::TaskLists>, ApiErrorCode>
        result) {
  // TODO(b/323975767): Generalize these histograms to the Tasks API.
  base::UmaHistogramTimes("Ash.Glanceables.Api.Tasks.GetTaskLists.Latency",
                          base::Time::Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));

  if (!result.has_value()) {
    RunGetTaskListsCallbacks(FetchStatus::kNotFresh, result.error(),
                             /*accumulated_raw_task_lists=*/{});
    return;
  }

  accumulated_raw_task_lists.insert(
      accumulated_raw_task_lists.end(),
      std::make_move_iterator(result.value()->mutable_items()->begin()),
      std::make_move_iterator(result.value()->mutable_items()->end()));

  if (result.value()->next_page_token().empty()) {
    // TODO(b/323975767): Generalize these histograms to the Tasks API.
    base::UmaHistogramCounts100(
        "Ash.Glanceables.Api.Tasks.GetTaskLists.PagesCount", page_number);
    base::UmaHistogramCounts100("Ash.Glanceables.Api.Tasks.TaskListsCount",
                                accumulated_raw_task_lists.size());
    // Get the fresh data from the Google Task API request successfully.
    RunGetTaskListsCallbacks(FetchStatus::kFresh, ApiErrorCode::HTTP_SUCCESS,
                             std::move(accumulated_raw_task_lists));
  } else {
    FetchTaskListsPage(result.value()->next_page_token(), page_number + 1,
                       std::move(accumulated_raw_task_lists));
  }
}

void TasksClientImpl::FetchTasksPage(
    const std::string& task_list_id,
    const std::string& page_token,
    int page_number,
    std::vector<std::unique_ptr<google_apis::tasks::Task>>
        accumulated_raw_tasks) {
  auto* const request_sender = GetRequestSender();
  request_sender->StartRequestWithAuthRetry(std::make_unique<ListTasksRequest>(
      request_sender, task_list_id, page_token, /*include_assigned=*/
      features::IsGlanceablesTimeManagementTasksViewAssignedTasksEnabled(),
      base::BindOnce(&TasksClientImpl::OnTasksPageFetched,
                     weak_factory_.GetWeakPtr(), task_list_id,
                     std::move(accumulated_raw_tasks), base::Time::Now(),
                     page_number)));

  if (tasks_request_callback_) {
    tasks_request_callback_.Run(task_list_id, page_token);
  }
}

void TasksClientImpl::OnTasksPageFetched(
    const std::string& task_list_id,
    std::vector<std::unique_ptr<google_apis::tasks::Task>>
        accumulated_raw_tasks,
    const base::Time& request_start_time,
    int page_number,
    base::expected<std::unique_ptr<google_apis::tasks::Tasks>, ApiErrorCode>
        result) {
  // TODO(b/323975767): Generalize these histograms to the Tasks API.
  base::UmaHistogramTimes("Ash.Glanceables.Api.Tasks.GetTasks.Latency",
                          base::Time::Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Tasks.GetTasks.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));

  if (!result.has_value()) {
    RunGetTasksCallbacks(task_list_id, FetchStatus::kNotFresh, result.error(),
                         /*accumulated_raw_tasks=*/{});
    return;
  }

  accumulated_raw_tasks.insert(
      accumulated_raw_tasks.end(),
      std::make_move_iterator(result.value()->mutable_items()->begin()),
      std::make_move_iterator(result.value()->mutable_items()->end()));

  if (result.value()->next_page_token().empty()) {
    // TODO(b/323975767): Generalize these histograms to the Tasks API.
    base::UmaHistogramCounts100("Ash.Glanceables.Api.Tasks.GetTasks.PagesCount",
                                page_number);
    base::UmaHistogramCounts100("Ash.Glanceables.Api.Tasks.RawTasksCount",
                                accumulated_raw_tasks.size());
    // Get the fresh data from the Google Task API request successfully.
    RunGetTasksCallbacks(task_list_id, FetchStatus::kFresh,
                         ApiErrorCode::HTTP_SUCCESS,
                         std::move(accumulated_raw_tasks));
  } else {
    FetchTasksPage(task_list_id, result.value()->next_page_token(),
                   page_number + 1, std::move(accumulated_raw_tasks));
  }
}

void TasksClientImpl::RunGetTaskListsCallbacks(
    FetchStatus final_fetch_status,
    std::optional<google_apis::ApiErrorCode> http_error,
    std::vector<std::unique_ptr<google_apis::tasks::TaskList>>
        accumulated_raw_task_lists) {
  task_lists_fetch_state_.status = final_fetch_status;
  if (final_fetch_status == FetchStatus::kFresh) {
    task_lists_.DeleteAll();

    // Gather existing cached task lists, and clear the ones that are no longer
    // present in the task list.
    std::set<std::string> abandoned_task_lists;
    for (const auto& task_list : tasks_in_task_lists_) {
      abandoned_task_lists.insert(task_list.first);
    }
    for (const auto& fetch_state : tasks_fetch_state_) {
      abandoned_task_lists.insert(fetch_state.first);
    }

    for (const auto& raw_item : accumulated_raw_task_lists) {
      abandoned_task_lists.erase(raw_item->id());
      task_lists_.Add(std::make_unique<TaskList>(
          raw_item->id(), raw_item->title(), raw_item->updated()));
    }

    for (const std::string& task_list_id : abandoned_task_lists) {
      tasks_in_task_lists_.erase(task_list_id);
      tasks_fetch_state_.erase(task_list_id);
    }
  }

  std::vector<GetTaskListsCallback> callbacks;
  task_lists_fetch_state_.callbacks.swap(callbacks);

  for (auto& callback : callbacks) {
    std::move(callback).Run(
        /*success=*/final_fetch_status == FetchStatus::kFresh, http_error,
        &task_lists_);
  }
}

void TasksClientImpl::RunGetTasksCallbacks(
    const std::string& task_list_id,
    FetchStatus final_fetch_status,
    std::optional<google_apis::ApiErrorCode> http_error,
    std::vector<std::unique_ptr<google_apis::tasks::Task>>
        accumulated_raw_tasks) {
  auto fetch_state_it = tasks_fetch_state_.find(task_list_id);
  if (fetch_state_it == tasks_fetch_state_.end()) {
    return;
  }

  const auto iter = tasks_in_task_lists_.find(task_list_id);
  TasksFetchState* fetch_state = fetch_state_it->second.get();

  if (final_fetch_status == FetchStatus::kFresh &&
      iter != tasks_in_task_lists_.end()) {
    iter->second.DeleteAll();

    for (auto& item : ConvertTasks(accumulated_raw_tasks)) {
      iter->second.Add(std::move(item));
    }

    // TODO(b/323975767): Generalize this histogram to the Tasks API.
    base::UmaHistogramCounts100("Ash.Glanceables.Api.Tasks.ProcessedTasksCount",
                                iter->second.item_count());
    fetch_state->last_updated_time = base::Time::Now();
  }

  fetch_state->status = final_fetch_status;

  std::vector<GetTasksCallback> callbacks;
  fetch_state->callbacks.swap(callbacks);

  const auto* task_list =
      iter != tasks_in_task_lists_.end() ? &iter->second : &stub_task_list_;
  for (auto& callback : callbacks) {
    std::move(callback).Run(
        /*success=*/final_fetch_status == FetchStatus::kFresh, http_error,
        task_list);
  }
}

void TasksClientImpl::OnMarkedAsCompleted(
    const base::Time& request_start_time,
    base::RepeatingClosure on_done,
    base::expected<std::unique_ptr<google_apis::tasks::Task>, ApiErrorCode>
        result) {
  // TODO(b/323975767): Generalize these histograms to the Tasks API.
  base::UmaHistogramTimes("Ash.Glanceables.Api.Tasks.PatchTask.Latency",
                          base::Time::Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Tasks.PatchTask.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));
  on_done.Run();
}

void TasksClientImpl::OnTaskAdded(
    const std::string& task_list_id,
    const base::Time& request_start_time,
    TasksClient::OnTaskSavedCallback callback,
    base::expected<std::unique_ptr<google_apis::tasks::Task>, ApiErrorCode>
        result) {
  // TODO(b/323975767): Generalize these histograms to the Tasks API.
  base::UmaHistogramTimes("Ash.Glanceables.Api.Tasks.InsertTask.Latency",
                          base::Time::Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Tasks.InsertTask.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));

  if (!result.has_value()) {
    std::move(callback).Run(/*http_error=*/result.error(), /*task=*/nullptr);
    return;
  }

  const auto iter = tasks_in_task_lists_.find(task_list_id);
  if (iter == tasks_in_task_lists_.end()) {
    std::move(callback).Run(/*http_error=*/ApiErrorCode::HTTP_SUCCESS,
                            /*task=*/nullptr);
    return;
  }

  const auto* const task = iter->second.AddAt(
      /*index=*/0,
      std::make_unique<Task>(result.value()->id(), result.value()->title(),
                             /*due=*/std::nullopt, /*completed=*/false,
                             /*has_subtasks=*/false,
                             /*has_email_link=*/false, /*has_notes=*/false,
                             result.value()->updated(),
                             result.value()->web_view_link(),
                             Task::OriginSurfaceType::kRegular));
  std::move(callback).Run(/*http_error=*/ApiErrorCode::HTTP_SUCCESS,
                          /*task=*/task);
}

void TasksClientImpl::OnTaskUpdated(
    const std::string& task_list_id,
    const base::Time& request_start_time,
    TasksClient::OnTaskSavedCallback callback,
    base::expected<std::unique_ptr<google_apis::tasks::Task>, ApiErrorCode>
        result) {
  // TODO(b/323975767): Generalize these histograms to the Tasks API.
  base::UmaHistogramTimes("Ash.Glanceables.Api.Tasks.PatchTask.Latency",
                          base::Time::Now() - request_start_time);
  base::UmaHistogramSparse("Ash.Glanceables.Api.Tasks.PatchTask.Status",
                           result.error_or(ApiErrorCode::HTTP_SUCCESS));

  if (!result.has_value()) {
    std::move(callback).Run(/*http_error=*/result.error(), /*task=*/nullptr);
    return;
  }

  const auto tasks_iter = tasks_in_task_lists_.find(task_list_id);
  if (tasks_iter == tasks_in_task_lists_.end()) {
    std::move(callback).Run(/*http_error=*/ApiErrorCode::HTTP_SUCCESS,
                            /*task=*/nullptr);
    return;
  }

  const auto* const result_data = result->get();
  const auto task_iter =
      std::find_if(tasks_iter->second.begin(), tasks_iter->second.end(),
                   [&result_data](const auto& task) {
                     return task->id == result_data->id();
                   });
  if (task_iter == tasks_iter->second.end()) {
    std::move(callback).Run(/*http_error=*/ApiErrorCode::HTTP_SUCCESS,
                            /*task=*/nullptr);
    return;
  }

  auto* const task = task_iter->get();
  task->title = result_data->title();
  task->completed = result_data->status() == TaskStatus::kCompleted;
  task->updated = result_data->updated();
  std::move(callback).Run(/*http_error=*/ApiErrorCode::HTTP_SUCCESS,
                          /*task=*/task);
}

google_apis::RequestSender* TasksClientImpl::GetRequestSender() {
  if (!request_sender_) {
    CHECK(create_request_sender_callback_);
    request_sender_ = std::move(create_request_sender_callback_)
                          .Run({GaiaConstants::kTasksReadOnlyOAuth2Scope,
                                GaiaConstants::kTasksOAuth2Scope},
                               traffic_annotation_tag_);
    CHECK(request_sender_);
  }
  return request_sender_.get();
}

}  // namespace ash::api
