// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"

#include "base/command_line.h"
#include "chrome/browser/ui/file_system_access_dialogs.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

bool RequestsAreIdentical(
    const FileSystemAccessPermissionRequestManager::RequestData& a,
    const FileSystemAccessPermissionRequestManager::RequestData& b) {
  return a.origin == b.origin && a.path == b.path &&
         a.handle_type == b.handle_type && a.access == b.access;
}

bool RequestsAreForSamePath(
    const FileSystemAccessPermissionRequestManager::RequestData& a,
    const FileSystemAccessPermissionRequestManager::RequestData& b) {
  return a.origin == b.origin && a.path == b.path &&
         a.handle_type == b.handle_type;
}

}  // namespace

struct FileSystemAccessPermissionRequestManager::Request {
  Request(
      RequestData data,
      base::OnceCallback<void(permissions::PermissionAction result)> callback,
      base::ScopedClosureRunner fullscreen_block)
      : data(std::move(data)) {
    callbacks.push_back(std::move(callback));
    fullscreen_blocks.push_back(std::move(fullscreen_block));
  }

  RequestData data;
  std::vector<base::OnceCallback<void(permissions::PermissionAction result)>>
      callbacks;
  std::vector<base::ScopedClosureRunner> fullscreen_blocks;
};

FileSystemAccessPermissionRequestManager::
    ~FileSystemAccessPermissionRequestManager() = default;

void FileSystemAccessPermissionRequestManager::AddRequest(
    RequestData data,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    base::ScopedClosureRunner fullscreen_block) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          permissions::switches::kDenyPermissionPrompts)) {
    std::move(callback).Run(permissions::PermissionAction::DENIED);
    return;
  }

  // Check if any pending requests are identical to the new request.
  if (current_request_ && RequestsAreIdentical(current_request_->data, data)) {
    current_request_->callbacks.push_back(std::move(callback));
    current_request_->fullscreen_blocks.push_back(std::move(fullscreen_block));
    return;
  }
  for (const auto& request : queued_requests_) {
    if (RequestsAreIdentical(request->data, data)) {
      request->callbacks.push_back(std::move(callback));
      request->fullscreen_blocks.push_back(std::move(fullscreen_block));
      return;
    }
    if (RequestsAreForSamePath(request->data, data)) {
      // This means access levels are different. Change the existing request
      // to kReadWrite, and add the new callback.
      request->data.access = Access::kReadWrite;
      request->callbacks.push_back(std::move(callback));
      request->fullscreen_blocks.push_back(std::move(fullscreen_block));
      return;
    }
  }

  queued_requests_.push_back(std::make_unique<Request>(
      std::move(data), std::move(callback), std::move(fullscreen_block)));
  if (!IsShowingRequest())
    ScheduleShowRequest();
}

FileSystemAccessPermissionRequestManager::
    FileSystemAccessPermissionRequestManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<FileSystemAccessPermissionRequestManager>(
          *web_contents) {}

bool FileSystemAccessPermissionRequestManager::CanShowRequest() const {
  // Deley showing requests until the main frame is fully loaded.
  // ScheduleShowRequest() will be called again when that happens.
  return web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame() &&
         !queued_requests_.empty() && !current_request_;
}

void FileSystemAccessPermissionRequestManager::ScheduleShowRequest() {
  if (!CanShowRequest())
    return;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FileSystemAccessPermissionRequestManager::DequeueAndShowRequest,
          weak_factory_.GetWeakPtr()));
}

void FileSystemAccessPermissionRequestManager::DequeueAndShowRequest() {
  if (!CanShowRequest())
    return;

  current_request_ = std::move(queued_requests_.front());
  queued_requests_.pop_front();

  if (auto_response_for_test_) {
    OnPermissionDialogResult(*auto_response_for_test_);
    return;
  }

  ShowFileSystemAccessPermissionDialog(
      current_request_->data,
      base::BindOnce(
          &FileSystemAccessPermissionRequestManager::OnPermissionDialogResult,
          weak_factory_.GetWeakPtr()),
      web_contents());
}

void FileSystemAccessPermissionRequestManager::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  // This is scheduled because while all calls to the browser have been
  // issued at DOMContentLoaded, they may be bouncing around in scheduled
  // callbacks finding the UI thread still. This makes sure we allow those
  // scheduled calls to AddRequest to complete before we show the page-load
  // permissions bubble.
  if (!queued_requests_.empty()) {
    ScheduleShowRequest();
  }
}

void FileSystemAccessPermissionRequestManager::OnPermissionDialogResult(
    permissions::PermissionAction result) {
  DCHECK(current_request_);
  for (auto& callback : current_request_->callbacks)
    std::move(callback).Run(result);
  current_request_ = nullptr;
  if (!queued_requests_.empty())
    ScheduleShowRequest();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FileSystemAccessPermissionRequestManager);
