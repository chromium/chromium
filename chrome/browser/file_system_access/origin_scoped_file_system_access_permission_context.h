// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_ORIGIN_SCOPED_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_ORIGIN_SCOPED_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_

#include <map>
#include <vector>

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

// Chrome implementation of FileSystemAccessPermissionContext. This concrete
// subclass of ChromeFileSystemAccessPermissionContext implements a permission
// model where permissions are shared across an entire origin. When the last tab
// for an origin is closed all permissions for that origin are revoked.
//
// All methods must be called on the UI thread.
class OriginScopedFileSystemAccessPermissionContext
    : public ChromeFileSystemAccessPermissionContext {
 public:
  explicit OriginScopedFileSystemAccessPermissionContext(
      content::BrowserContext* context);
  ~OriginScopedFileSystemAccessPermissionContext() override;

  // content::FileSystemAccessPermissionContext:
  scoped_refptr<content::FileSystemAccessPermissionGrant>
  GetReadPermissionGrant(const url::Origin& origin,
                         const base::FilePath& path,
                         HandleType handle_type,
                         UserAction user_action) override;
  scoped_refptr<content::FileSystemAccessPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const base::FilePath& path,
                          HandleType handle_type,
                          UserAction user_action) override;

  // ChromeFileSystemAccessPermissionContext:
  Grants GetPermissionGrants(const url::Origin& origin) override;
  void RevokeGrants(const url::Origin& origin) override;
  bool OriginHasReadAccess(const url::Origin& origin) override;
  bool OriginHasWriteAccess(const url::Origin& origin) override;
  void NavigatedAwayFromOrigin(const url::Origin& origin) override;

  content::BrowserContext* profile() const { return profile_; }

  void TriggerTimersForTesting();

 private:
  class PermissionGrantImpl;
  void PermissionGrantDestroyed(PermissionGrantImpl* grant);

  // Schedules triggering all open windows to update their File System Access
  // usage indicator icon. Multiple calls to this method can result in only a
  // single actual update.
  void ScheduleUsageIconUpdate();

  // Updates the File System Access usage indicator icon in all currently open
  // windows.
  void DoUsageIconUpdate();

  // Checks if any tabs are open for |origin|, and if not revokes all
  // permissions for that origin.
  void MaybeCleanupPermissions(const url::Origin& origin);

  base::WeakPtr<ChromeFileSystemAccessPermissionContext> GetWeakPtr() override;

  content::BrowserContext* const profile_;

  // Permission state per origin.
  struct OriginState;
  std::map<url::Origin, OriginState> origins_;

  bool usage_icon_update_scheduled_ = false;

  base::WeakPtrFactory<OriginScopedFileSystemAccessPermissionContext>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_ORIGIN_SCOPED_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
