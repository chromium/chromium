// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"

#include "base/command_line.h"
#include "chrome/browser/ui/file_system_access/file_system_access_dialogs.h"
#include "chrome/browser/ui/file_system_access/file_system_access_permission_dialog.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

using FileRequestData =
    FileSystemAccessPermissionRequestManager::FileRequestData;
using RequestData = FileSystemAccessPermissionRequestManager::RequestData;
using RequestType = FileSystemAccessPermissionRequestManager::RequestType;

bool AllFileRequestDataAreIdentical(const std::vector<FileRequestData>& a,
                                    const std::vector<FileRequestData>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); i++) {
    if (a[i].path_info.path != b[i].path_info.path ||
        a[i].handle_type != b[i].handle_type || a[i].access != b[i].access) {
      return false;
    }
  }
  return true;
}

bool RequestsAreIdentical(const RequestData& a, const RequestData& b) {
  return a.origin == b.origin && a.request_type == b.request_type &&
         AllFileRequestDataAreIdentical(a.file_request_data,
                                        b.file_request_data);
}

bool NewPermissionRequestsAreForSamePath(const RequestData& a,
                                         const RequestData& b) {
  return a.origin == b.origin &&
         a.request_type == RequestType::kNewPermission &&
         a.request_type == b.request_type && a.file_request_data.size() == 1 &&
         a.file_request_data.size() == b.file_request_data.size() &&
         a.file_request_data[0].path_info.path ==
             b.file_request_data[0].path_info.path &&
         a.file_request_data[0].handle_type ==
             b.file_request_data[0].handle_type;
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

FileSystemAccessPermissionRequestManager::RequestData::RequestData(
    RequestType request_type,
    const url::Origin& origin,
    const std::vector<FileRequestData>& file_request_data)
    : request_type(request_type),
      origin(origin),
      file_request_data(file_request_data) {}
FileSystemAccessPermissionRequestManager::RequestData::~RequestData() = default;
FileSystemAccessPermissionRequestManager::RequestData::RequestData(
    RequestData&&) = default;
FileSystemAccessPermissionRequestManager::RequestData::RequestData(
    const RequestData&) = default;

void FileSystemAccessPermissionRequestManager::AddRequest(
    RequestData data,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    base::ScopedClosureRunner fullscreen_block) {
  if (data.request_type == RequestType::kNewPermission) {
    DCHECK(data.file_request_data.size() == 1);
  }

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
    if (NewPermissionRequestsAreForSamePath(request->data, data)) {
      // This means access levels are different. Change the existing request
      // to kReadWrite, and add the new callback.
      request->data.file_request_data[0].access = Access::kReadWrite;
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

  switch (current_request_->data.request_type) {
    case RequestType::kNewPermission:
      ShowFileSystemAccessPermissionDialog(
          current_request_->data,
          base::BindOnce(&FileSystemAccessPermissionRequestManager::
                             OnPermissionDialogResult,
                         weak_factory_.GetWeakPtr()),
          web_contents());
      break;
    case RequestType::kRestorePermissions:
      ShowFileSystemAccessRestorePermissionDialog(
          current_request_->data,
          base::BindOnce(&FileSystemAccessPermissionRequestManager::
                             OnPermissionDialogResult,
                         weak_factory_.GetWeakPtr()),
          web_contents());
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
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
