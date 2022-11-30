// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/background_refresh_task.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/feed/android/refresh_task_scheduler_impl.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/feed_feature_list.h"

namespace feed {

BackgroundRefreshTask::BackgroundRefreshTask(RefreshTaskId task_id)
    : task_id_(task_id) {}
BackgroundRefreshTask::~BackgroundRefreshTask() {}

void BackgroundRefreshTask::OnStartTaskInReducedMode(
    const background_task::TaskParameters& task_params,
    background_task::TaskFinishedCallback callback,
    SimpleFactoryKey* key) {
  callback_ = std::move(callback);
}

void BackgroundRefreshTask::OnStartTaskWithFullBrowser(
    const background_task::TaskParameters& task_params,
    background_task::TaskFinishedCallback callback,
    content::BrowserContext* browser_context) {
  Run(std::move(callback), browser_context);
}

void BackgroundRefreshTask::OnFullBrowserLoaded(
    content::BrowserContext* browser_context) {
  // This function is only called after |OnStartTaskInReducedMode()|,
  // but the callback might have been cleared by |OnStopTask()|.
  if (callback_)
    Run(std::move(callback_), browser_context);
}

bool BackgroundRefreshTask::OnStopTask(
    const background_task::TaskParameters& task_params) {
  callback_.Reset();
  return true;  // Reschedule.
}

// Kicks off the Feed refresh. Called only after the full browser is started.
void BackgroundRefreshTask::Run(background_task::TaskFinishedCallback callback,
                                content::BrowserContext* browser_context) {
  if (!FeedService::IsEnabled(
          *Profile::FromBrowserContext(browser_context)->GetPrefs()))
    return std::move(callback).Run(false);

  FeedService* service =
      FeedServiceFactory::GetForBrowserContext(browser_context);
  if (!service)
    return std::move(callback).Run(false);

  RefreshTaskSchedulerImpl* task_scheduler =
      static_cast<RefreshTaskSchedulerImpl*>(
          service->GetRefreshTaskScheduler());

  task_scheduler->Run(task_id_, service,
                      base::BindOnce(std::move(callback), false));
}

}  // namespace feed
