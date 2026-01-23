// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/android/background_upload_task.h"

#include "components/background_task_scheduler/background_task.h"
#include "components/background_task_scheduler/task_parameters.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_context.h"

namespace metrics {

BackgroundUploadTask::BackgroundUploadTask(background_task::TaskIds task_id)
    : task_id_(task_id) {}
BackgroundUploadTask::~BackgroundUploadTask() = default;

void BackgroundUploadTask::OnStartTaskInReducedMode(
    const background_task::TaskParameters& task_params,
    background_task::TaskFinishedCallback callback,
    SimpleFactoryKey* key) {
  // TODO(crbug.com/445735421): Implement.
}

void BackgroundUploadTask::OnStartTaskWithFullBrowser(
    const background_task::TaskParameters& task_params,
    background_task::TaskFinishedCallback callback,
    content::BrowserContext* browser_context) {
  // TODO(crbug.com/445735421): Implement.
}

void BackgroundUploadTask::OnFullBrowserLoaded(
    content::BrowserContext* browser_context) {
  // TODO(crbug.com/445735421): Implement.
}

bool BackgroundUploadTask::OnStopTask(
    const background_task::TaskParameters& task_params) {
  // TODO(crbug.com/445735421): Implement.
  return false;
}

}  // namespace metrics
