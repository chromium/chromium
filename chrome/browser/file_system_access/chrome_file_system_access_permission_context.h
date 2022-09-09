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
#include "base/timer/timer.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class HostContentSettingsMap;
enum ContentSetting;

namespace content {
class BrowserContext;
}  // namespace content

namespace features {
// Enables persistent permissions for the File System Access API.
extern const base::Feature kFileSystemAccessPersistentPermissions;
}  // namespace features

// Chrome implementation of FileSystemAccessPermissionContext. This class
// implements a permission model where permissions are shared across an entire
// origin.
//
// There are two orthogonal permission models at work in this class:
// 1. Active permissions are scoped to the lifetime of the handles that
//    reference the grants. When the last tab for an origin is closed, all
//    active permissions for that origin are revoked.
// 2. Persistent permissions allow for auto-granting permissions which the user
//    had given access to prior, within a given time window. These are stored
//    using ObjectPermissionContextBase.
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
  std::string GetKeyForObject(const base::Value& object) override;
  bool IsValidObject(const base::Value& object) override;
  std::u16string GetObjectDisplayName(const base::Value& object) override;

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
      ui::SelectFileDialog::Type dialog_type,
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

  ContentSetting GetReadGuardContentSetting(const url::Origin& origin);
  ContentSetting GetWriteGuardContentSetting(const url::Origin& origin);

  void SetMaxIdsPerOriginForTesting(unsigned int max_ids) {
    max_ids_per_origin_ = max_ids;
  }

  enum class GrantType { kRead, kWrite };

  enum class PersistedPermissionOptions {
    kDoNotUpdatePersistedPermission,
    kUpdatePersistedPermission,
  };

  // Returns a snapshot of both the currently granted active and persisted
  // permissions.
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
  void RevokeGrants(const url::Origin& origin,
                    PersistedPermissionOptions persisted_status);

  // Returns whether active permissions exist for the origin of the given type.
  bool OriginHasReadAccess(const url::Origin& origin);
  bool OriginHasWriteAccess(const url::Origin& origin);

  // Called by FileSystemAccessTabHelper when a top-level frame was navigated
  // away from |origin| to some other origin. Is virtual for testing purposes.
  virtual void NavigatedAwayFromOrigin(const url::Origin& origin);

  content::BrowserContext* profile() const { return profile_; }

  void TriggerTimersForTesting();

  // Return all persisted objects, including those which have expired.
  std::vector<std::unique_ptr<ObjectPermissionContextBase::Object>>
  GetAllGrantedOrExpiredObjects();
  void UpdatePersistedPermissionsForTesting();
  bool HasPersistedPermissionForTesting(const url::Origin& origin,
                                        const base::FilePath& path,
                                        HandleType handle_type,
                                        GrantType grant_type);

  HostContentSettingsMap* content_settings() { return content_settings_.get(); }

  // This long after the handle has last been used, revoke the persisted
  // permission.
  static constexpr base::TimeDelta
      kPersistentPermissionExpirationTimeoutDefault = base::Hours(5);
  static constexpr base::TimeDelta
      kPersistentPermissionExpirationTimeoutExtended = base::Days(30);
  // Amount of time a persisted permission will remain persisted after its
  // expiry. Used for metrics.
  static constexpr base::TimeDelta kPersistentPermissionGracePeriod =
      base::Days(1);

 protected:
  SEQUENCE_CHECKER(sequence_checker_);

  base::RepeatingTimer&
  periodic_sweep_persisted_permissions_timer_for_testing() {
    return periodic_sweep_persisted_permissions_timer_;
  }

  // Returns whether persisted permission grants for the origin are subject to
  // the extended permission duration policy.
  bool OriginHasExtendedPermissions(const url::Origin& origin);

 private:
  enum class MetricsOptions { kRecord, kDoNotRecord };

  class PermissionGrantImpl;
  void PermissionGrantDestroyed(PermissionGrantImpl* grant);

  void DidConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      const base::FilePath& path,
      HandleType handle_type,
      ui::SelectFileDialog::Type dialog_type,
      content::GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveEntryResult)> callback,
      bool should_block);

  void MaybeMigrateOriginToNewSchema(const url::Origin& origin);

  // An origin can only specify up to `max_ids_per_origin_` custom IDs per
  // origin (not including the default ID). If this limit is exceeded, evict
  // using LRU.
  void MaybeEvictEntries(base::Value& value);

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

  // Sweeps HostContentSettingsMap, revoking expired persisted permissions and
  // auto-extending persisted permissions with active grants.
  void UpdatePersistedPermissions();
  // Only sweep persisted permissions for the given |origin|.
  void UpdatePersistedPermissionsForOrigin(const url::Origin& origin);

  // Renew the persisted permission if it has active permissions, or
  // revoke the persisted permission if it has expired.
  void MaybeRenewOrRevokePersistedPermission(const url::Origin& origin,
                                             base::Value grant,
                                             bool has_extended_permissions);

  bool AncestorHasActivePermission(const url::Origin& origin,
                                   const base::FilePath& path,
                                   GrantType grant_type);
  absl::optional<base::Value> GetPersistedPermission(
      const url::Origin& origin,
      const base::FilePath& path);
  bool HasPersistedPermission(const url::Origin& origin,
                              const base::FilePath& path,
                              HandleType handle_type,
                              GrantType grant_type,
                              MetricsOptions options);
  bool PersistentPermissionIsExpired(const base::Time& last_used,
                                     bool has_extended_permissions);

  base::WeakPtr<ChromeFileSystemAccessPermissionContext> GetWeakPtr();

  const raw_ptr<content::BrowserContext> profile_;

  // Permission state per origin.
  struct OriginState;
  std::map<url::Origin, OriginState> origins_;

  bool usage_icon_update_scheduled_ = false;

  scoped_refptr<HostContentSettingsMap> content_settings_;

  // Number of custom IDs an origin can specify.
  size_t max_ids_per_origin_ = 32u;

  const raw_ptr<const base::Clock> clock_;
  base::RepeatingTimer periodic_sweep_persisted_permissions_timer_;

  base::WeakPtrFactory<ChromeFileSystemAccessPermissionContext> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_CHROME_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
