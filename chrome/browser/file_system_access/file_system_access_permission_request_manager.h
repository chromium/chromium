// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_REQUEST_MANAGER_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_REQUEST_MANAGER_H_

#include "base/containers/circular_deque.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace permissions {
enum class PermissionAction;
}

// This class manages File System Access permission requests for a particular
// WebContents. It is very similar to the generic PermissionRequestManager
// class, and as such deals with throttling, coallescing and/or completely
// denying permission requests, depending on the situation and policy.
//
// File System Access code doesn't just use PermissionRequestManager directly
// because the permission requests use different UI, and as such can't easily
// be supported by PermissionRequestManager.
//
// The FileSystemAccessPermissionRequestManager should be used on the UI thread.
//
// TODO(crbug.com/40101962): Add test for this class.
class FileSystemAccessPermissionRequestManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          FileSystemAccessPermissionRequestManager> {
 public:
  ~FileSystemAccessPermissionRequestManager() override;

  enum class RequestType {
    // Requests to get new permission for single file or directory handle.
    kNewPermission,

    // Requests to restore permission for multiple file or directory handles.
    kRestorePermissions,
  };

  enum class Access {
    // Only ask for read access.
    kRead,
    // Only ask for write access, assuming read access has already been granted.
    kWrite,
    // Ask for both read and write access.
    kReadWrite
  };

  struct FileRequestData {
    FileRequestData(
        const content::PathInfo& path_info,
        content::FileSystemAccessPermissionContext::HandleType handle_type,
        Access access)
        : path_info(path_info), handle_type(handle_type), access(access) {}
    ~FileRequestData() = default;
    FileRequestData(FileRequestData&&) = default;
    FileRequestData(const FileRequestData&) = default;
    FileRequestData& operator=(FileRequestData&&) = default;
    FileRequestData& operator=(const FileRequestData&) = default;

    content::PathInfo path_info;
    content::FileSystemAccessPermissionContext::HandleType handle_type;
    Access access;
  };

  struct RequestData {
    RequestData(RequestType request_type,
                const url::Origin& origin,
                const std::vector<FileRequestData>& file_request_data);
    ~RequestData();
    RequestData(RequestData&&);
    RequestData(const RequestData&);
    RequestData& operator=(RequestData&&) = default;
    RequestData& operator=(const RequestData&) = default;

    RequestType request_type;
    url::Origin origin;
    std::vector<FileRequestData> file_request_data;
  };

  void AddRequest(
      RequestData request,
      base::OnceCallback<void(permissions::PermissionAction result)> callback,
      base::ScopedClosureRunner fullscreen_block);

  // Do NOT use this method in production code. Use this method in browser
  // tests that need to accept or deny permissions when requested in
  // JavaScript. Your test needs to call this before permission is requested,
  // and then the bubble will proceed as desired as soon as it would have been
  // shown.
  void set_auto_response_for_test(
      std::optional<permissions::PermissionAction> response) {
    auto_response_for_test_ = response;
  }

 private:
  friend class content::WebContentsUserData<
      FileSystemAccessPermissionRequestManager>;

  explicit FileSystemAccessPermissionRequestManager(
      content::WebContents* web_contents);

  bool IsShowingRequest() const { return current_request_ != nullptr; }
  bool CanShowRequest() const;
  void ScheduleShowRequest();
  void DequeueAndShowRequest();

  // WebContentsObserver
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  void OnPermissionDialogResult(permissions::PermissionAction result);

  struct Request;
  // Request currently being shown in prompt.
  std::unique_ptr<Request> current_request_;
  // Queued up requests.
  base::circular_deque<std::unique_ptr<Request>> queued_requests_;

  std::optional<permissions::PermissionAction> auto_response_for_test_;

  base::WeakPtrFactory<FileSystemAccessPermissionRequestManager> weak_factory_{
      this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_REQUEST_MANAGER_H_
