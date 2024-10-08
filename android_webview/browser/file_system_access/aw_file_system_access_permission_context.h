//// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_FILE_SYSTEM_ACCESS_AW_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_FILE_SYSTEM_ACCESS_AW_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/file_system_access_permission_context.h"

namespace android_webview {

// This class is the Android WebView implementation of
// content::FileSystemAccessPermissionContext. It controls permissions for
// which files have read / write access.
// All these methods must always be called on the UI thread.
//
// This implementation allows read and write to all files, with a blocklist for
// files that are internal to the chromium engine.
//
// In WebView, all files are opened via WebChromeClient#onFileChooser() which
// is where the hosting app can filter any files that they do not want to allow
// web code to access. If files have been provided, then we assume that read
// and write access is allowed.
class AwFileSystemAccessPermissionContext
    : public content::FileSystemAccessPermissionContext {
 public:
  AwFileSystemAccessPermissionContext();
  AwFileSystemAccessPermissionContext(
      const AwFileSystemAccessPermissionContext&) = delete;
  AwFileSystemAccessPermissionContext& operator=(
      const AwFileSystemAccessPermissionContext&) = delete;
  ~AwFileSystemAccessPermissionContext() override;

  // content::FileSystemAccessPermissionContext:
  scoped_refptr<content::FileSystemAccessPermissionGrant>
  GetReadPermissionGrant(const url::Origin& origin,
                         const content::PathInfo& path_info,
                         HandleType handle_type,
                         UserAction user_action) override;
  scoped_refptr<content::FileSystemAccessPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const content::PathInfo& path_info,
                          HandleType handle_type,
                          UserAction user_action) override;
  void ConfirmSensitiveEntryAccess(
      const url::Origin& origin,
      const content::PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action,
      content::GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveEntryResult)> callback) override;
  void PerformAfterWriteChecks(
      std::unique_ptr<content::FileSystemAccessWriteItem> item,
      content::GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) override;
  bool CanObtainReadPermission(const url::Origin& origin) override;
  bool CanObtainWritePermission(const url::Origin& origin) override;
  bool IsFileTypeDangerous(const base::FilePath& path,
                           const url::Origin& origin) override;
  void SetLastPickedDirectory(const url::Origin& origin,
                              const std::string& id,
                              const content::PathInfo& path_info) override;
  content::PathInfo GetLastPickedDirectory(const url::Origin& origin,
                                           const std::string& id) override;
  base::FilePath GetWellKnownDirectoryPath(
      blink::mojom::WellKnownDirectory directory,
      const url::Origin& origin) override;
  std::u16string GetPickerTitle(
      const blink::mojom::FilePickerOptionsPtr& options) override;
  void NotifyEntryMoved(const url::Origin& origin,
                        const content::PathInfo& old_path,
                        const content::PathInfo& new_path) override;
  void OnFileCreatedFromShowSaveFilePicker(
      const GURL& file_picker_binding_context,
      const storage::FileSystemURL& url) override;
  void CheckPathsAgainstEnterprisePolicy(
      std::vector<content::PathInfo> entries,
      content::GlobalRenderFrameHostId frame_id,
      EntriesAllowedByEnterprisePolicyCallback callback) override;

 private:
  // Checks whether the file or directory at `path` corresponds to a directory
  // WebView considers sensitive (i.e. system files). Calls `callback` with
  // whether the path is on the blocklist.
  void CheckPathAgainstBlocklist(const base::FilePath& path,
                                 base::OnceCallback<void(bool)> callback);
  void DidCheckPathAgainstBlocklist(
      const base::FilePath& path,
      base::OnceCallback<void(SensitiveEntryResult)> callback,
      bool should_block);

  base::WeakPtrFactory<AwFileSystemAccessPermissionContext> weak_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_FILE_SYSTEM_ACCESS_AW_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
