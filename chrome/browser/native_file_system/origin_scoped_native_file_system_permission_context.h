// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_FILE_SYSTEM_ORIGIN_SCOPED_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_NATIVE_FILE_SYSTEM_ORIGIN_SCOPED_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_

#include <map>
#include <vector>

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

// Chrome implementation of NativeFileSystemPermissionContext. This concrete
// subclass of ChromeNativeFileSystemPermissionContext implements a permission
// model where permissions are shared across an entire origin. When the last tab
// for an origin is closed all permissions for that origin are revoked.
//
// All methods must be called on the UI thread.
class OriginScopedNativeFileSystemPermissionContext
    : public ChromeNativeFileSystemPermissionContext {
 public:
  explicit OriginScopedNativeFileSystemPermissionContext(
      content::BrowserContext* context);
  ~OriginScopedNativeFileSystemPermissionContext() override;

  // content::NativeFileSystemPermissionContext:
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetReadPermissionGrant(const url::Origin& origin,
                         const base::FilePath& path,
                         HandleType handle_type,
                         UserAction user_action) override;
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const base::FilePath& path,
                          HandleType handle_type,
                          UserAction user_action) override;

  // ChromeNativeFileSystemPermissionContext:
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

  // Schedules triggering all open windows to update their native file system
  // usage indicator icon. Multiple calls to this method can result in only a
  // single actual update.
  void ScheduleUsageIconUpdate();

  // Updates the native file system usage indicator icon in all currently open
  // windows.
  void DoUsageIconUpdate();

  // Checks if any tabs are open for |origin|, and if not revokes all
  // permissions for that origin.
  void MaybeCleanupPermissions(const url::Origin& origin);

  base::WeakPtr<ChromeNativeFileSystemPermissionContext> GetWeakPtr() override;

  content::BrowserContext* const profile_;

  // Permission state per origin.
  struct OriginState;
  std::map<url::Origin, OriginState> origins_;

  bool usage_icon_update_scheduled_ = false;

  base::WeakPtrFactory<OriginScopedNativeFileSystemPermissionContext>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_ORIGIN_SCOPED_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
