// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_REQUEST_MANAGER_H_
#define CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_REQUEST_MANAGER_H_

#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

enum class PermissionAction;

// This class manages native file system permission requests for a particular
// WebContents. It is very similar to the generic PermissionRequestManager
// class, and as such deals with throttling, coallescing and/or completely
// denying permission requests, depending on the situation and policy.
//
// Native File System code doesn't just use PermissionRequestManager directly
// because the permission requests use different UI, and as such can't easily
// be supported by PermissionRequestManager.
//
// The NativeFileSystemPermissionRequestManager should be used on the UI thread.
class NativeFileSystemPermissionRequestManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          NativeFileSystemPermissionRequestManager> {
 public:
  ~NativeFileSystemPermissionRequestManager() override;

  struct RequestData {
    RequestData(const url::Origin& origin,
                const base::FilePath& path,
                bool is_directory)
        : origin(origin), path(path), is_directory(is_directory) {}
    RequestData(RequestData&&) = default;
    RequestData& operator=(RequestData&&) = default;

    url::Origin origin;
    base::FilePath path;
    bool is_directory;
  };

  void AddRequest(RequestData request,
                  base::OnceCallback<void(PermissionAction result)> callback);

  // Do NOT use this method in production code. Use this method in browser
  // tests that need to accept or deny permissions when requested in
  // JavaScript. Your test needs to call this before permission is requested,
  // and then the bubble will proceed as desired as soon as it would have been
  // shown.
  void set_auto_response_for_test(base::Optional<PermissionAction> response) {
    auto_response_for_test_ = response;
  }

 private:
  friend class content::WebContentsUserData<
      NativeFileSystemPermissionRequestManager>;

  explicit NativeFileSystemPermissionRequestManager(
      content::WebContents* web_contents);

  bool IsShowingRequest() const { return current_request_ != nullptr; }
  bool CanShowRequest() const;
  void ScheduleShowRequest();
  void DequeueAndShowRequest();

  // WebContentsObserver
  void DocumentOnLoadCompletedInMainFrame() override;

  void OnPermissionDialogResult(PermissionAction result);

  struct Request;
  // Request currently being shown in prompt.
  std::unique_ptr<Request> current_request_;
  // Queued up requests.
  base::circular_deque<std::unique_ptr<Request>> queued_requests_;

  // We only show new prompts when this is true.
  bool main_frame_has_fully_loaded_ = false;

  base::Optional<PermissionAction> auto_response_for_test_;

  base::WeakPtrFactory<NativeFileSystemPermissionRequestManager> weak_factory_{
      this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_REQUEST_MANAGER_H_
