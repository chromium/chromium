// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_CHROME_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_CHROME_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_

#include <map>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"

class HostContentSettingsMap;
enum ContentSetting;

namespace content {
class BrowserContext;
}  // namespace content

namespace features {
// Enables persistent permissions for the File System Access API.
BASE_DECLARE_FEATURE(kFileSystemAccessPersistentPermissions);

#if BUILDFLAG(IS_WIN)
// Enables blocking local UNC path on Windows for the File System Access API.
BASE_DECLARE_FEATURE(kFileSystemAccessLocalUNCPathBlock);
#endif
}  // namespace features

// Chrome implementation of FileSystemAccessPermissionContext. This class
// implements a permission model where permissions are shared across an entire
// origin.
//
// There are two orthogonal permission models at work in this class:
// 1. Active permissions are scoped to the lifetime of the handles that
//    reference the grants. When the last tab for an origin is closed, all
//    active permissions for that origin are revoked.
// 2. Persistent permissions, which are stored via ObjectPermissionContextBase,
//    allow for auto-granting permissions that the user had given access to
//    prior. Before user accepts the Extend Permission prompt, the permission
//    objects are simply "dormant grants", representing recently granted
//    permission, which are created together with active permissions. After
//    user accepts the Extend Permission prompt, dormant grants become
//    "extended grants", which can auto-grant permissions.
//
// All methods must be called on the UI thread.
class ChromeFileSystemAccessPermissionContext
    : public content::FileSystemAccessPermissionContext,
      public permissions::ObjectPermissionContextBase {
 public:
  explicit ChromeFileSystemAccessPermissionContext(
      content::BrowserContext* context,
      const base::Clock* clock = base::DefaultClock::GetInstance());
  ChromeFileSystemAccessPermissionContext(
      const ChromeFileSystemAccessPermissionContext&) = delete;
  ChromeFileSystemAccessPermissionContext& operator=(
      const ChromeFileSystemAccessPermissionContext&) = delete;
  ~ChromeFileSystemAccessPermissionContext() override;

  // permissions::ObjectPermissionContextBase
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& origin) override;
  std::vector<std::unique_ptr<Object>> GetAllGrantedObjects() override;
  std::string GetKeyForObject(const base::Value::Dict& object) override;
  bool IsValidObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;
  std::set<url::Origin> GetOriginsWithGrants() override;

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
  void ConfirmSensitiveEntryAccess(
      const url::Origin& origin,
      PathType path_type,
      const base::FilePath& path,
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

  void SetLastPickedDirectory(const url::Origin& origin,
                              const std::string& id,
                              const base::FilePath& path,
                              const PathType type) override;
  PathInfo GetLastPickedDirectory(const url::Origin& origin,
                                  const std::string& id) override;
  base::FilePath GetWellKnownDirectoryPath(
      blink::mojom::WellKnownDirectory directory,
      const url::Origin& origin) override;

  std::u16string GetPickerTitle(
      const blink::mojom::FilePickerOptionsPtr& options) override;

  void NotifyEntryMoved(const url::Origin& origin,
                        const base::FilePath& old_path,
                        const base::FilePath& new_path) override;

  ContentSetting GetReadGuardContentSetting(const url::Origin& origin) const;
  ContentSetting GetWriteGuardContentSetting(const url::Origin& origin) const;

  void SetMaxIdsPerOriginForTesting(unsigned int max_ids) {
    max_ids_per_origin_ = max_ids;
  }

  // This method may only be called when the Persistent Permissions feature
  // flag is enabled.
  void SetOriginHasExtendedPermissionForTesting(const url::Origin& origin) {
    CHECK(base::FeatureList::IsEnabled(
        features::kFileSystemAccessPersistentPermissions));
    // TODO(crbug.com/1011533): Refactor to use the registered Content Setting
    // value, once implemented.
    extended_permissions_settings_map_[origin] =
        ContentSetting::CONTENT_SETTING_ALLOW;
  }
  bool RevokeActiveGrantsForTesting(
      const url::Origin& origin,
      base::FilePath file_path = base::FilePath()) {
    return RevokeActiveGrants(origin, std::move(file_path));
  }
  std::vector<std::unique_ptr<Object>> GetExtendedPersistedObjectsForTesting(
      const url::Origin& origin) {
    return GetExtendedPersistedObjects(origin);
  }
  std::vector<std::unique_ptr<Object>> GetDormantPersistedObjectsForTesting(
      const url::Origin& origin) {
    return GetDormantPersistedObjects(origin);
  }

  enum class GrantType { kRead, kWrite };

  // Converts permissions objects into a snapshot of grants categorized by
  // read/write and file/directory types. Currently, used in UI code.
  // Assumes that all objects are grants for the same origin.
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
  Grants ConvertObjectsToGrants(
      const std::vector<std::unique_ptr<Object>> objects);

  // Revokes active and extended grants for the given origin and given file
  // path.
  void RevokeGrant(const url::Origin& origin, const base::FilePath& file_path);

  // Revokes active and extended grants for the given origin.
  void RevokeGrants(const url::Origin& origin);

  // Returns whether active permissions exist for the origin of the given type.
  bool OriginHasReadAccess(const url::Origin& origin);
  bool OriginHasWriteAccess(const url::Origin& origin);

  // Called by FileSystemAccessTabHelper when a top-level frame was navigated
  // away from |origin| to some other origin. Is virtual for testing purposes.
  virtual void NavigatedAwayFromOrigin(const url::Origin& origin);

  content::BrowserContext* profile() const { return profile_; }

  void TriggerTimersForTesting();

  scoped_refptr<content::FileSystemAccessPermissionGrant>
  GetExtendedReadPermissionGrantForTesting(const url::Origin& origin,
                                           const base::FilePath& path,
                                           HandleType handle_type);
  scoped_refptr<content::FileSystemAccessPermissionGrant>
  GetExtendedWritePermissionGrantForTesting(const url::Origin& origin,
                                            const base::FilePath& path,
                                            HandleType handle_type);
  bool HasExtendedPermissionForTesting(const url::Origin& origin,
                                       const base::FilePath& path,
                                       HandleType handle_type,
                                       GrantType grant_type);

  HostContentSettingsMap* content_settings() { return content_settings_.get(); }

  // Dictionary key for the FILE_SYSTEM_ACCESS_CHOOSER_DATA setting.
  // This key is defined in this header file because it is used both in
  // the chrome_file_system_access_permission_context and the
  // site_settings_helper, which displays File System Access permissions on the
  // chrome://settings/content/filesystem UI.
  static constexpr char kPermissionPathKey[] = "path";

 protected:
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  class PermissionGrantImpl;

  // This value should not be stored, and should only be used to check the
  // state of persisted grants, using the `GetPersistedGrantState()` method.
  enum class PersistedGrantState {
    // Represents a grant that was granted access on previous visit.
    // Extended Permissions is not enabled for the given origin.
    kDormant,
    // Represents a grant that "shadows" an active grant for the
    // current visit. Extended permissions is not enabled for the
    // given origin. Shadow grants can be used to auto-grant
    // permission requests. May have active grants that are GRANTED.
    kShadow,
    // Represents a grant that persists across multiple visits.
    // The user has enabled Extended Permissions for the given
    // origin via the Restore Prompt or by installing a PWA. Can be
    // used to auto-grant permission requests.
    kExtended,
  };

  enum class PersistedPermissionOptions {
    kDoNotUpdatePersistedPermission,
    kUpdatePersistedPermission,
  };

  // Retrieve the persisted grant state for all persisted grants for a given
  // origin.
  PersistedGrantState GetPersistedGrantState(const url::Origin& origin) const;

  void PermissionGrantDestroyed(PermissionGrantImpl* grant);

  // Checks whether the file or directory at `path` corresponds to a directory
  // Chrome considers sensitive (i.e. system files). Calls `callback` with
  // whether the path is on the blocklist.
  void CheckPathAgainstBlocklist(PathType path_type,
                                 const base::FilePath& path,
                                 HandleType handle_type,
                                 base::OnceCallback<void(bool)> callback);
  void DidCheckPathAgainstBlocklist(
      const url::Origin& origin,
      const base::FilePath& path,
      HandleType handle_type,
      UserAction user_action,
      content::GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveEntryResult)> callback,
      bool should_block);

  void MaybeMigrateOriginToNewSchema(const url::Origin& origin);

  // An origin can only specify up to `max_ids_per_origin_` custom IDs per
  // origin (not including the default ID). If this limit is exceeded, evict
  // using LRU.
  void MaybeEvictEntries(base::Value::Dict& dict);

  // Schedules triggering all open windows to update their File System Access
  // usage indicator icon. Multiple calls to this method can result in only a
  // single actual update.
  void ScheduleUsageIconUpdate();

  // Updates the File System Access usage indicator icon in all currently open
  // windows.
  void DoUsageIconUpdate();

  // Checks if any tabs are open for |origin|, and if not revokes all active
  // permissions for that origin.
  void MaybeCleanupActivePermissions(const url::Origin& origin);

  bool AncestorHasActivePermission(const url::Origin& origin,
                                   const base::FilePath& path,
                                   GrantType grant_type) const;

  bool OriginHasExtendedPermission(const url::Origin& origin) const;

  // Returns whether the origin has extended permission for a specific file.
  bool HasExtendedPermission(const url::Origin& origin,
                             const base::FilePath& path,
                             HandleType handle_type,
                             GrantType grant_type);

  bool HasGrantedActiveGrant(const url::Origin& origin) const;

  // Similar to GetGrantedObjects() but returns only extended grants.
  std::vector<std::unique_ptr<Object>> GetExtendedPersistedObjects(
      const url::Origin& origin);

  // Similar to GetGrantedObjects() but returns only dormant grants.
  std::vector<std::unique_ptr<Object>> GetDormantPersistedObjects(
      const url::Origin& origin);

  // Revokes the active grants for the given origin, and returns whether any is
  // revoked. If the `file_path` is provided, then only the grant matching
  // the file path is revoked.
  bool RevokeActiveGrants(const url::Origin& origin,
                          base::FilePath file_path = base::FilePath());

  base::WeakPtr<ChromeFileSystemAccessPermissionContext> GetWeakPtr();

  const raw_ptr<content::BrowserContext> profile_;

  // Permission state per origin.
  struct OriginState;
  std::map<url::Origin, OriginState> active_permissions_map_;

  // TODO(crbug.com/1011533): Remove this map once the Persistent Permission
  // Content Setting is implemented.
  std::map<url::Origin, ContentSetting> extended_permissions_settings_map_;

  bool usage_icon_update_scheduled_ = false;

  scoped_refptr<HostContentSettingsMap> content_settings_;

  // Number of custom IDs an origin can specify.
  size_t max_ids_per_origin_ = 32u;

  const raw_ptr<const base::Clock> clock_;

  base::WeakPtrFactory<ChromeFileSystemAccessPermissionContext> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_CHROME_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
