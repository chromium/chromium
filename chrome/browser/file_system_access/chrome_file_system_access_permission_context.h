// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_CHROME_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_CHROME_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_

#include <map>
#include <vector>

#include "base/sequence_checker.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class HostContentSettingsMap;
enum ContentSetting;

namespace content {
class BrowserContext;
}  // namespace content

// Chrome implementation of FileSystemAccessPermissionContext. This class
// implements a permission model where permissions are shared across an entire
// origin. When the last tab for an origin is closed all permissions for that
// origin are revoked.
//
// All methods must be called on the UI thread.
//
// This class does not inherit from ChooserContextBase because the model this
// API uses doesn't really match what ChooserContextBase has to provide. The
// limited lifetime of File System Access permission grants (scoped to the
// lifetime of the handles that reference the grants), and the possible
// interactions between grants for directories and grants for children of those
// directories as well as possible interactions between read and write grants
// make it harder to squeeze this into a shape that fits with
// ChooserContextBase.
class ChromeFileSystemAccessPermissionContext
    : public content::FileSystemAccessPermissionContext,
      public KeyedService {
 public:
  explicit ChromeFileSystemAccessPermissionContext(
      content::BrowserContext* context);
  ~ChromeFileSystemAccessPermissionContext() override;

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
  void ConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      PathType path_type,
      const base::FilePath& path,
      HandleType handle_type,
      content::GlobalFrameRoutingId frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback) override;
  void PerformAfterWriteChecks(
      std::unique_ptr<content::FileSystemAccessWriteItem> item,
      content::GlobalFrameRoutingId frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) override;
  bool CanObtainReadPermission(const url::Origin& origin) override;
  bool CanObtainWritePermission(const url::Origin& origin) override;

  void SetLastPickedDirectory(const url::Origin& origin,
                              const std::string& id,
                              const base::FilePath& path,
                              const PathType type) override;
  PathInfo GetLastPickedDirectory(const url::Origin& origin,
                                  const std::string& id) override;
  base::FilePath GetWellKnownDirectoryPath(
      blink::mojom::WellKnownDirectory directory) override;

  ContentSetting GetReadGuardContentSetting(const url::Origin& origin);
  ContentSetting GetWriteGuardContentSetting(const url::Origin& origin);

  void SetMaxIdsPerOriginForTesting(unsigned int max_ids) {
    max_ids_per_origin_ = max_ids;
  }

  // Returns a snapshot of the currently granted permissions.
  // TODO(https://crbug.com/984769): Eliminate process_id and frame_id from this
  // method when grants stop being scoped to a frame.
  struct Grants {
    Grants();
    ~Grants();
    Grants(Grants&&);
    Grants& operator=(Grants&&);

    std::vector<base::FilePath> file_read_grants;
    std::vector<base::FilePath> file_write_grants;
    std::vector<base::FilePath> directory_read_grants;
    std::vector<base::FilePath> directory_write_grants;
  };
  Grants GetPermissionGrants(const url::Origin& origin);

  // Revokes write access and directory read access for the given origin.
  void RevokeGrants(const url::Origin& origin);

  bool OriginHasReadAccess(const url::Origin& origin);
  bool OriginHasWriteAccess(const url::Origin& origin);

  // Called by FileSystemAccessTabHelper when a top-level frame was navigated
  // away from |origin| to some other origin.
  void NavigatedAwayFromOrigin(const url::Origin& origin);

  content::BrowserContext* profile() const { return profile_; }

  void TriggerTimersForTesting();

  HostContentSettingsMap* content_settings() { return content_settings_.get(); }

 protected:
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  class PermissionGrantImpl;
  void PermissionGrantDestroyed(PermissionGrantImpl* grant);

  void DidConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      const base::FilePath& path,
      HandleType handle_type,
      content::GlobalFrameRoutingId frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback,
      bool should_block);

  void MaybeMigrateOriginToNewSchema(const url::Origin& origin);

  // An origin can only specify up to `max_ids_per_origin_` custom IDs per
  // origin (not including the default ID). If this limit is exceeded, evict
  // using LRU.
  void MaybeEvictEntries(std::unique_ptr<base::Value>& value);

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

  base::WeakPtr<ChromeFileSystemAccessPermissionContext> GetWeakPtr();

  content::BrowserContext* const profile_;

  // Permission state per origin.
  struct OriginState;
  std::map<url::Origin, OriginState> origins_;

  bool usage_icon_update_scheduled_ = false;

  scoped_refptr<HostContentSettingsMap> content_settings_;

  // Number of custom IDs an origin can specify.
  size_t max_ids_per_origin_ = 32u;

  base::WeakPtrFactory<ChromeFileSystemAccessPermissionContext> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ChromeFileSystemAccessPermissionContext);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_CHROME_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
