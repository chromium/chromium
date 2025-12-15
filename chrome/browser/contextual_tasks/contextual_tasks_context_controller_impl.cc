// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_impl.h"

#include <optional>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/buildflag.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/core/session_id.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace contextual_tasks {

namespace {

size_t GetNumberOfActiveTasks(Profile* profile) {
  size_t number_of_active_tasks = 0;
#if !BUILDFLAG(IS_ANDROID)
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [profile, &number_of_active_tasks](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile) {
          return true;
        }

        // Check how many tasks are cached in the side panel.
        if (auto* coordinator =
                ContextualTasksSidePanelCoordinator::From(browser)) {
          number_of_active_tasks += coordinator->GetNumberOfActiveTasks();
        }

        // Check how many tasks are open in a full tab.
        TabStripModel* tab_strip_model = browser->GetTabStripModel();
        if (tab_strip_model) {
          for (int i = 0; i < tab_strip_model->count(); ++i) {
            if (auto* web_contents = tab_strip_model->GetWebContentsAt(i)) {
              if (web_contents->GetLastCommittedURL().scheme() ==
                      content::kChromeUIScheme &&
                  web_contents->GetLastCommittedURL().host() ==
                      chrome::kChromeUIContextualTasksHost) {
                number_of_active_tasks++;
              }
            }
          }
        }

        return true;
      });
#endif  // !BUILDFLAG(IS_ANDROID)
  return number_of_active_tasks;
}

void RecordNumberOfActiveTasks(Profile* profile) {
  // Note that we are adding 1 to the number of active tasks because the
  // histogram is recorded before the task is created, but it's not immediately
  // reflected in the active tasks count.
  base::UmaHistogramCounts100("ContextualTasks.ActiveTasksCount",
                              GetNumberOfActiveTasks(profile) + 1);
}

}  // namespace

ContextualTasksContextControllerImpl::ContextualTasksContextControllerImpl(
    Profile* profile,
    ContextualTasksService* service)
    : profile_(profile), service_(service) {}

ContextualTasksContextControllerImpl::~ContextualTasksContextControllerImpl() =
    default;

FeatureEligibility
ContextualTasksContextControllerImpl::GetFeatureEligibility() {
  return service_->GetFeatureEligibility();
}

bool ContextualTasksContextControllerImpl::IsInitialized() {
  return service_->IsInitialized();
}

ContextualTask ContextualTasksContextControllerImpl::CreateTask() {
  RecordNumberOfActiveTasks(profile_);
  return service_->CreateTask();
}

ContextualTask ContextualTasksContextControllerImpl::CreateTaskFromUrl(
    const GURL& url) {
  RecordNumberOfActiveTasks(profile_);
  return service_->CreateTaskFromUrl(url);
}

void ContextualTasksContextControllerImpl::GetTaskById(
    const base::Uuid& task_id,
    base::OnceCallback<void(std::optional<ContextualTask>)> callback) const {
  service_->GetTaskById(task_id, std::move(callback));
}

void ContextualTasksContextControllerImpl::GetTasks(
    base::OnceCallback<void(std::vector<ContextualTask>)> callback) const {
  service_->GetTasks(std::move(callback));
}

void ContextualTasksContextControllerImpl::DeleteTask(
    const base::Uuid& task_id) {
  service_->DeleteTask(task_id);
}

void ContextualTasksContextControllerImpl::UpdateThreadForTask(
    const base::Uuid& task_id,
    ThreadType thread_type,
    const std::string& server_id,
    std::optional<std::string> conversation_turn_id,
    std::optional<std::string> title) {
  service_->UpdateThreadForTask(task_id, thread_type, server_id,
                                conversation_turn_id, title);
}

void ContextualTasksContextControllerImpl::RemoveThreadFromTask(
    const base::Uuid& task_id,
    ThreadType type,
    const std::string& server_id) {
  service_->RemoveThreadFromTask(task_id, type, server_id);
}

std::optional<ContextualTask>
ContextualTasksContextControllerImpl::GetTaskFromServerId(
    ThreadType thread_type,
    const std::string& server_id) {
  return service_->GetTaskFromServerId(thread_type, server_id);
}

void ContextualTasksContextControllerImpl::AttachUrlToTask(
    const base::Uuid& task_id,
    const GURL& url) {
  service_->AttachUrlToTask(task_id, url);
}

void ContextualTasksContextControllerImpl::DetachUrlFromTask(
    const base::Uuid& task_id,
    const GURL& url) {
  service_->DetachUrlFromTask(task_id, url);
}

void ContextualTasksContextControllerImpl::SetUrlResourcesFromServer(
    const base::Uuid& task_id,
    std::vector<UrlResource> url_resources) {
  service_->SetUrlResourcesFromServer(task_id, std::move(url_resources));
}

void ContextualTasksContextControllerImpl::GetContextForTask(
    const base::Uuid& task_id,
    const std::set<ContextualTaskContextSource>& sources,
    std::unique_ptr<ContextDecorationParams> params,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  service_->GetContextForTask(task_id, sources, std::move(params),
                              std::move(context_callback));
}

void ContextualTasksContextControllerImpl::AssociateTabWithTask(
    const base::Uuid& task_id,
    SessionID tab_id) {
  service_->AssociateTabWithTask(task_id, tab_id);
}

void ContextualTasksContextControllerImpl::DisassociateTabFromTask(
    const base::Uuid& task_id,
    SessionID tab_id) {
  service_->DisassociateTabFromTask(task_id, tab_id);
}

void ContextualTasksContextControllerImpl::DisassociateAllTabsFromTask(
    const base::Uuid& task_id) {
  service_->DisassociateAllTabsFromTask(task_id);
}

std::optional<ContextualTask>
ContextualTasksContextControllerImpl::GetContextualTaskForTab(
    SessionID tab_id) const {
  return service_->GetContextualTaskForTab(tab_id);
}

std::vector<SessionID>
ContextualTasksContextControllerImpl::GetTabsAssociatedWithTask(
    const base::Uuid& task_id) const {
  return service_->GetTabsAssociatedWithTask(task_id);
}

void ContextualTasksContextControllerImpl::ClearAllTabAssociationsForTask(
    const base::Uuid& task_id) {
  service_->ClearAllTabAssociationsForTask(task_id);
}

void ContextualTasksContextControllerImpl::AddObserver(Observer* observer) {
  service_->AddObserver(observer);
}

void ContextualTasksContextControllerImpl::RemoveObserver(Observer* observer) {
  service_->RemoveObserver(observer);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
ContextualTasksContextControllerImpl::GetAiThreadControllerDelegate() {
  return service_->GetAiThreadControllerDelegate();
}

}  // namespace contextual_tasks
