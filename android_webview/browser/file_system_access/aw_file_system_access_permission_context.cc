// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/file_system_access/aw_file_system_access_permission_context.h"

#include <vector>

#include "base/android/build_info.h"
#include "base/base_paths_android.h"
#include "base/base_paths_posix.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/file_system_access_permission_grant.h"

namespace android_webview {
namespace {

class FixedFileSystemAccessPermissionGrant
    : public content::FileSystemAccessPermissionGrant {
 public:
  FixedFileSystemAccessPermissionGrant(PermissionStatus status,
                                       content::PathInfo path_info)
      : status_(status), path_info_(std::move(path_info)) {}

  FixedFileSystemAccessPermissionGrant(
      const FixedFileSystemAccessPermissionGrant&) = delete;

  FixedFileSystemAccessPermissionGrant& operator=(
      const FixedFileSystemAccessPermissionGrant&) = delete;

  // FileSystemAccessPermissionGrant:
  PermissionStatus GetStatus() override { return status_; }

  base::FilePath GetPath() override { return path_info_.path; }

  std::string GetDisplayName() override { return path_info_.display_name; }

  void RequestPermission(
      content::GlobalRenderFrameHostId frame_id,
      UserActivationState user_activation_state,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override {
    std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
  }

 private:
  ~FixedFileSystemAccessPermissionGrant() override = default;

  const PermissionStatus status_;
  const content::PathInfo path_info_;
};

bool ShouldBlockAccessToPath(const base::FilePath& path) {
  CHECK(!path.empty());

  // We do not block any Content-URIs since no internal chrome paths are
  // exposed. We rely on the hosting app to filter any disallowed paths when
  // they implement WebChromeClient#onFileChooser().
  if (path.IsContentUri()) {
    return false;
  }

  CHECK(path.IsAbsolute());

  constexpr int kBlockedPaths[] = {
      base::DIR_ANDROID_APP_DATA,
      base::DIR_CACHE,
  };

  // Check internal WebView paths.
  for (auto path_key : kBlockedPaths) {
    base::FilePath blocked_path;
    if (!base::PathService::Get(path_key, &blocked_path)) {
      continue;
    }
    if (path == blocked_path || blocked_path.IsParent(path)) {
      return true;
    }
  }

  return false;
}

}  //  namespace

AwFileSystemAccessPermissionContext::AwFileSystemAccessPermissionContext() =
    default;

AwFileSystemAccessPermissionContext::~AwFileSystemAccessPermissionContext() =
    default;

scoped_refptr<content::FileSystemAccessPermissionGrant>
AwFileSystemAccessPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const content::PathInfo& path_info,
    HandleType handle_type,
    UserAction user_action) {
  return base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      content::FileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
      path_info);
}

scoped_refptr<content::FileSystemAccessPermissionGrant>
AwFileSystemAccessPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const content::PathInfo& path_info,
    HandleType handle_type,
    UserAction user_action) {
  return base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      content::FileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
      path_info);
}

void AwFileSystemAccessPermissionContext::ConfirmSensitiveEntryAccess(
    const url::Origin& origin,
    const content::PathInfo& path_info,
    HandleType handle_type,
    UserAction user_action,
    content::GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(SensitiveEntryResult)> callback) {
  CheckPathAgainstBlocklist(
      path_info.path,
      base::BindOnce(
          &AwFileSystemAccessPermissionContext::DidCheckPathAgainstBlocklist,
          weak_factory_.GetWeakPtr(), path_info.path, std::move(callback)));
}

void AwFileSystemAccessPermissionContext::PerformAfterWriteChecks(
    std::unique_ptr<content::FileSystemAccessWriteItem> item,
    content::GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(AfterWriteCheckResult)> callback) {
  std::move(callback).Run(AfterWriteCheckResult::kAllow);
}

bool AwFileSystemAccessPermissionContext::CanObtainReadPermission(
    const url::Origin& origin) {
  return true;
}
bool AwFileSystemAccessPermissionContext::CanObtainWritePermission(
    const url::Origin& origin) {
  return true;
}

bool AwFileSystemAccessPermissionContext::IsFileTypeDangerous(
    const base::FilePath& path,
    const url::Origin& origin) {
  return false;
}

void AwFileSystemAccessPermissionContext::SetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id,
    const content::PathInfo& path_info) {}

content::PathInfo AwFileSystemAccessPermissionContext::GetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id) {
  return content::PathInfo();
}

base::FilePath AwFileSystemAccessPermissionContext::GetWellKnownDirectoryPath(
    blink::mojom::WellKnownDirectory directory,
    const url::Origin& origin) {
  return base::FilePath();
}

std::u16string AwFileSystemAccessPermissionContext::GetPickerTitle(
    const blink::mojom::FilePickerOptionsPtr& options) {
  return std::u16string();
}

void AwFileSystemAccessPermissionContext::NotifyEntryMoved(
    const url::Origin& origin,
    const content::PathInfo& old_path,
    const content::PathInfo& new_path) {}

void AwFileSystemAccessPermissionContext::OnFileCreatedFromShowSaveFilePicker(
    const GURL& file_picker_binding_context,
    const storage::FileSystemURL& url) {}

void AwFileSystemAccessPermissionContext::CheckPathsAgainstEnterprisePolicy(
    std::vector<content::PathInfo> entries,
    content::GlobalRenderFrameHostId frame_id,
    EntriesAllowedByEnterprisePolicyCallback callback) {
  std::move(callback).Run(std::move(entries));
}

void AwFileSystemAccessPermissionContext::CheckPathAgainstBlocklist(
    const base::FilePath& path,
    base::OnceCallback<void(bool)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ShouldBlockAccessToPath, path), std::move(callback));
}

void AwFileSystemAccessPermissionContext::DidCheckPathAgainstBlocklist(
    const base::FilePath& path,
    base::OnceCallback<void(SensitiveEntryResult)> callback,
    bool should_block) {
  std::move(callback).Run(should_block ? SensitiveEntryResult::kAbort
                                       : SensitiveEntryResult::kAllowed);
}

}  // namespace android_webview
