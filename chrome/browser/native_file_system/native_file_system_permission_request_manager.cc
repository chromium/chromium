// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"

#include "base/command_line.h"
#include "base/task/post_task.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_task_traits.h"

bool operator==(
    const NativeFileSystemPermissionRequestManager::RequestData& a,
    const NativeFileSystemPermissionRequestManager::RequestData& b) {
  return a.origin == b.origin && a.path == b.path &&
         a.is_directory == b.is_directory;
}

struct NativeFileSystemPermissionRequestManager::Request {
  Request(RequestData data,
          base::OnceCallback<void(PermissionAction result)> callback)
      : data(std::move(data)) {
    callbacks.push_back(std::move(callback));
  }

  const RequestData data;
  std::vector<base::OnceCallback<void(PermissionAction result)>> callbacks;
};

NativeFileSystemPermissionRequestManager::
    ~NativeFileSystemPermissionRequestManager() = default;

void NativeFileSystemPermissionRequestManager::AddRequest(
    RequestData data,
    base::OnceCallback<void(PermissionAction result)> callback) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDenyPermissionPrompts)) {
    std::move(callback).Run(PermissionAction::DENIED);
    return;
  }

  // Check if any pending requests are identical to the new request.
  if (current_request_ && current_request_->data == data) {
    current_request_->callbacks.push_back(std::move(callback));
    return;
  }
  for (const auto& request : queued_requests_) {
    if (request->data == data) {
      request->callbacks.push_back(std::move(callback));
      return;
    }
  }

  queued_requests_.push_back(
      std::make_unique<Request>(std::move(data), std::move(callback)));
  if (!IsShowingRequest())
    ScheduleShowRequest();
}

NativeFileSystemPermissionRequestManager::
    NativeFileSystemPermissionRequestManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

bool NativeFileSystemPermissionRequestManager::CanShowRequest() const {
  // Deley showing requests until the main frame is fully loaded.
  // ScheduleShowRequest() will be called again when that happens.
  return main_frame_has_fully_loaded_ && !queued_requests_.empty() &&
         !current_request_;
}

void NativeFileSystemPermissionRequestManager::ScheduleShowRequest() {
  if (!CanShowRequest())
    return;

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &NativeFileSystemPermissionRequestManager::DequeueAndShowRequest,
          weak_factory_.GetWeakPtr()));
}

void NativeFileSystemPermissionRequestManager::DequeueAndShowRequest() {
  if (!CanShowRequest())
    return;

  current_request_ = std::move(queued_requests_.front());
  queued_requests_.pop_front();

  if (auto_response_for_test_) {
    OnPermissionDialogResult(*auto_response_for_test_);
    return;
  }

  ShowNativeFileSystemPermissionDialog(
      current_request_->data.origin, current_request_->data.path,
      current_request_->data.is_directory,
      base::BindOnce(
          &NativeFileSystemPermissionRequestManager::OnPermissionDialogResult,
          weak_factory_.GetWeakPtr()),
      web_contents());
}

void NativeFileSystemPermissionRequestManager::
    DocumentOnLoadCompletedInMainFrame() {
  main_frame_has_fully_loaded_ = true;
  // This is scheduled because while all calls to the browser have been
  // issued at DOMContentLoaded, they may be bouncing around in scheduled
  // callbacks finding the UI thread still. This makes sure we allow those
  // scheduled calls to AddRequest to complete before we show the page-load
  // permissions bubble.
  if (!queued_requests_.empty())
    ScheduleShowRequest();
}

void NativeFileSystemPermissionRequestManager::OnPermissionDialogResult(
    PermissionAction result) {
  DCHECK(current_request_);
  for (auto& callback : current_request_->callbacks)
    std::move(callback).Run(result);
  current_request_ = nullptr;
  if (!queued_requests_.empty())
    ScheduleShowRequest();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NativeFileSystemPermissionRequestManager)
