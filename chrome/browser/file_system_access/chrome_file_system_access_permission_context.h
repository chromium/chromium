// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_CHROME_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_CHROME_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_

#include <map>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/permissions/features.h"
#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"

#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#endif

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "components/enterprise/common/files_scan_data.h"
#endif

class HostContentSettingsMap;
class OneTimePermissionsTracker;
enum ContentSetting;

namespace content {
class BrowserContext;
}  // namespace content

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
//    objects are simply "shadow grants" or "dormant grants", representing
//    recently granted permission, which are created together with active
//    permissions. After user accepts the Extend Permission prompt, dormant
//    grants become "extended grants", which can auto-grant permissions.
//
// All methods must be called on the UI thread.
class ChromeFileSystemAccessPermissionContext
    : public content::FileSystemAccessPermissionContext,
      public permissions::ObjectPermissionContextBase,
      public OneTimePermissionsTrackerObserver
#if !BUILDFLAG(IS_ANDROID)
    ,
      public web_app::WebAppInstallManagerObserver
#endif
{
 public:
  using FileCreatedFromShowSaveFilePickerCallbackList =
      base::RepeatingCallbackList<void(const GURL&,
                                       const storage::FileSystemURL&)>;

  // Represents the type of persisted grant. This value should not be stored
  // and should only be used to check the state of persisted grants,
  // using the `GetPersistedGrantType()` method.
  enum class PersistedGrantType {
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

  // Represents the origin-scoped state that helps determining
  // `PersistedGrantType`.
  enum class PersistedGrantStatus {
    // Origin state has been loaded, and persisted grants can may represent
    // dormant grants if they exist, or extended grants if the origin has
    // extended permission enabled.
    kLoaded,
    // Persisted grants are synced for this session and represent Shadow or
    // Extended grants.
    kCurrent,
    // Persisted grants are in dormant state due to being backgrounded.
    kBackgrounded
  };

  enum class GrantType { kRead, kWrite };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // TODO(crbug.com/40101962): Currently, the `kIgnored` outcome is not user-
  // detectable, and no metrics are expected to be recorded for this case.
  // Consider removing this value from the `RestorePermissionPromptOutcome`
  // enum when updating the corresponding logic in the permission context code.
  enum class RestorePermissionPromptOutcome {
    kAllowed = 0,
    kAllowedOnce = 1,
    kIgnored = 2,
    kRejected = 3,
    kDismissed = 4,
    kMaxValue = kDismissed
  };

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

  // OneTimePermissionsTrackerObserver:
  void OnAllTabsInBackgroundTimerExpired(
      const url::Origin& origin,
      const OneTimePermissionsTrackerObserver::BackgroundExpiryType&
          expiry_type) override;
  void OnLastPageFromOriginClosed(const url::Origin& origin) override;
  void OnShutdown() override;

#if !BUILDFLAG(IS_ANDROID)
  // WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  // TODO(crbug.com/340952100): Remove after the InstallState is saved in the
  // database & available from OnWebAppInstalled.
  void OnWebAppInstalledWithOsHooks(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
#endif

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
  bool IsFileTypeDangerous(const base::FilePath& path,
                           const url::Origin& origin) override;
  bool CanObtainReadPermission(const url::Origin& origin) override;
  bool CanObtainWritePermission(const url::Origin& origin) override;
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

  // Registers a subscriber to be notified of file creation events originating
  // from `window.showSaveFilePicker()` until the returned subscription is
  // destroyed.
  [[nodiscard]] base::CallbackListSubscription
  AddFileCreatedFromShowSaveFilePickerCallback(
      FileCreatedFromShowSaveFilePickerCallbackList::CallbackType callback);

  ContentSetting GetReadGuardContentSetting(const url::Origin& origin) const;
  ContentSetting GetWriteGuardContentSetting(const url::Origin& origin) const;

  std::vector<base::FilePath> GetGrantedPaths(const url::Origin& origin);

  void SetMaxIdsPerOriginForTesting(unsigned int max_ids) {
    max_ids_per_origin_ = max_ids;
  }

  PersistedGrantStatus GetPersistedGrantStatusForTesting(
      const url::Origin& origin) {
    CHECK(base::FeatureList::IsEnabled(
        features::kFileSystemAccessPersistentPermissions));
    return GetPersistedGrantStatus(origin);
  }

  std::vector<std::unique_ptr<Object>> GetExtendedPersistedObjectsForTesting(
      const url::Origin& origin) {
    return GetExtendedPersistedObjects(origin);
  }

  PersistedGrantType GetPersistedGrantTypeForTesting(
      const url::Origin& origin) {
    return GetPersistedGrantType(origin);
  }

  bool HasExtendedPermissionForTesting(const url::Origin& origin,
                                       const content::PathInfo& path_info,
                                       HandleType handle_type,
                                       GrantType grant_type) {
    // TODO(crbug.com/40101962): Clean up this usage in test.
    return CanAutoGrantViaPersistentPermission(origin, path_info.path,
                                               handle_type, grant_type);
  }

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
  Grants ConvertObjectsToGrants(std::vector<std::unique_ptr<Object>> objects);

  // Creates a new set of persisted grants based on the currently granted,
  // active grants for a given origin.
  void CreatePersistedGrantsFromActiveGrants(const url::Origin& origin);

  // Revokes `origin`'s active and extended grant for `file_path`. It does not
  // reset the extended permission state. Currently called from UI (i.e. Site
  // Settings page).
  void RevokeGrant(const url::Origin& origin, const base::FilePath& file_path);

  // Revokes `origin`'s active and extended grants, and resets the extended
  // permission state. Currently, called from UI (i.e. Site Settings page,
  // usage icon/bubble).
  void RevokeGrants(const url::Origin& origin);

  // Revokes all the active grants in `active_permissions_map_`. This method is
  // currently used by the browsing data clearning code.
  void RevokeAllActiveGrants();

  // Returns whether active or extended grants exist for the origin of the given
  // type.
  bool OriginHasReadAccess(const url::Origin& origin);
  bool OriginHasWriteAccess(const url::Origin& origin);

  // Returns whether the origin has extended permission enabled via user
  // opt-in or by having an actively installed PWA.
  bool OriginHasExtendedPermission(const url::Origin& origin);

  // Enable or disable extended permissions as a result of user
  // interaction with the File System Access Page Info UI.
  void SetOriginExtendedPermissionByUser(const url::Origin& origin);
  void RemoveOriginExtendedPermissionByUser(const url::Origin& origin);

  // Called by FileSystemAccessTabHelper when a top-level frame was navigated
  // away from `origin` to some other origin. Is virtual for testing purposes.
  virtual void NavigatedAwayFromOrigin(const url::Origin& origin);

  content::BrowserContext* profile() const { return profile_; }

  void TriggerTimersForTesting();

  void SetOriginHasExtendedPermissionForTesting(const url::Origin& origin);

  bool RevokeActiveGrantsForTesting(
      const url::Origin& origin,
      base::FilePath file_path = base::FilePath()) {
    return RevokeActiveGrants(origin, std::move(file_path));
  }

  scoped_refptr<content::FileSystemAccessPermissionGrant>
  GetExtendedReadPermissionGrantForTesting(const url::Origin& origin,
                                           const content::PathInfo& path_info,
                                           HandleType handle_type);
  scoped_refptr<content::FileSystemAccessPermissionGrant>
  GetExtendedWritePermissionGrantForTesting(const url::Origin& origin,
                                            const content::PathInfo& path_info,
                                            HandleType handle_type);

  HostContentSettingsMap* content_settings() { return content_settings_.get(); }

  // Dictionary key for the FILE_SYSTEM_ACCESS_CHOOSER_DATA setting.
  // This key is defined in this header file because it is used both in
  // the chrome_file_system_access_permission_context and the
  // site_settings_helper, which displays File System Access permissions on the
  // chrome://settings/content/filesystem UI.
  static constexpr char kPermissionPathKey[] = "path";

  // KeyedService:
  void Shutdown() override;

 protected:
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  class PermissionGrantImpl;

  enum class PersistedPermissionOptions {
    kDoNotUpdatePersistedPermission,
    kUpdatePersistedPermission,
  };

  enum class WebAppInstallStatus {
    kUnknown = 0,
    kInstalled,
    kUninstalled,
  };

  void PermissionGrantDestroyed(PermissionGrantImpl* grant);

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  void OnContentAnalysisComplete(
      std::vector<content::PathInfo> entries,
      EntriesAllowedByEnterprisePolicyCallback callback,
      std::vector<base::FilePath> paths,
      std::vector<bool> allowed);
#endif

  // Checks whether the file or directory at `path` corresponds to a directory
  // Chrome considers sensitive (i.e. system files). Calls `callback` with
  // whether the path is on the blocklist.
  void CheckPathAgainstBlocklist(const content::PathInfo& path_info,
                                 HandleType handle_type,
                                 base::OnceCallback<void(bool)> callback);
  void DidCheckPathAgainstBlocklist(
      const url::Origin& origin,
      const content::PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action,
      content::GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveEntryResult)> callback,
      bool should_block);

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

  // Checks if any tabs are open for the given origin, and if not, updates the
  // permission grants.
  void MaybeCleanupPermissions(const url::Origin& origin);

  void CleanupPermissions(const url::Origin& origin);

  bool AncestorHasActivePermission(const url::Origin& origin,
                                   const base::FilePath& path,
                                   GrantType grant_type) const;

  // Returns whether the grant has a `GRANTED` permission status.
  bool HasGrantedActivePermissionStatus(PermissionGrantImpl* grant) const;

  // Given the current state of the origin, returns whether it is eligible to
  // trigger the restore permission prompt instead of the permission request
  // prompt. All of the following criteria must meet:
  // 1) Origin is not embargoed for showing the Restore permission prompt for
  //    too many times
  // 2) Origin does not have extended permission yet
  // 3) Permission request is on a handle retrieved from Indexed DB, or any
  //    type of request after the permission is auto-revoked due to tabs
  //    being backgrounded.
  // 4) A dormant grant matching the requested file path and handle type exists.
  bool IsEligibleToUpgradePermissionRequestToRestorePrompt(
      const url::Origin& origin,
      const base::FilePath& file_path,
      HandleType handle_type,
      UserAction user_action,
      GrantType grant_type);

  // Builds a list of `FileRequestData` from persisted grants, which is used
  // to show the restore permission prompt. Expects that the persisted grants
  // are dormant grants eligible to be restored.
  std::vector<FileSystemAccessPermissionRequestManager::FileRequestData>
  GetFileRequestDataForRestorePermissionPrompt(const url::Origin& origin);

  // Called when the restore permission prompt is accepted as a result of the
  // user selecting the 'Allow on every visit' option.
  void OnRestorePermissionAllowedEveryTime(const url::Origin& origin);

  // Called when the restore permission prompt is accepted as a result of the
  // user selecting the 'Allow this time' option.
  void OnRestorePermissionAllowedOnce(const url::Origin& origin);

  // Called when the restore permission prompt is dismissed or denied.
  void OnRestorePermissionDeniedOrDismissed(const url::Origin& origin);

  // Records restore permission prompt ignore with
  // `PermissionDecisionAutoblocker`.
  void OnRestorePermissionIgnored(const url::Origin& origin);

  // Updates active and persisted grants when the user selects either the
  // 'Allow this time' or 'Allow on every visit' option from the restore
  // permission prompt. Assumes that persisted grants are dormant type.
  void UpdateGrantsOnRestorePermissionAllowed(const url::Origin& origin);

  // Updates active and persisted grants when the user denies, dismisses or
  // ignores the restore permission prompt. Assumes that persisted grants are
  // dormant type.
  void UpdateGrantsOnRestorePermissionNotAllowed(const url::Origin& origin);

  // Updates persist grants when the user responses to the permission prompt.
  void UpdateGrantsOnPermissionRequestResult(const url::Origin& origin);

  // Returns whether a matching persisted grant object exists.
  bool HasPersistedGrantObject(const url::Origin& origin,
                               const base::FilePath& file_path,
                               HandleType handle_type,
                               GrantType grant_type);

  // Returns whether a permission object value has matching fields.
  bool HasMatchingValue(const base::Value::Dict& value,
                        const base::FilePath& file_path,
                        HandleType handle_type,
                        GrantType grant_type);

  // Returns whether a file or directory can be auto-granted via persistent
  // permission.
  bool CanAutoGrantViaPersistentPermission(const url::Origin& origin,
                                           const base::FilePath& path,
                                           HandleType handle_type,
                                           GrantType grant_type);

  // Returns whether a file or directory can be auto-granted by having
  // ancestor with persistent permission.
  bool CanAutoGrantViaAncestorPersistentPermission(const url::Origin& origin,
                                                   const base::FilePath& path,
                                                   GrantType grant_type);

  // Removes extended permissions for grants. Does not update the content
  // setting type for extended permissions.
  // This method should only be called for an origin that already has extended
  // permissions.
  void RemoveExtendedPermission(const url::Origin& origin);
  // Upgrades permission grants to extended grants. Does not update the content
  // setting type for extended permissions.
  // This method should only be called for an origin that does not already
  // have extended permissions.
  void UpgradeToExtendedPermission(const url::Origin& origin);

  // Retrieve the persisted grant type for a given origin.
  PersistedGrantType GetPersistedGrantType(const url::Origin& origin);

  PersistedGrantStatus GetPersistedGrantStatus(const url::Origin& origin) const;
  void SetPersistedGrantStatus(const url::Origin& origin,
                               PersistedGrantStatus persisted_grant_status);

  // Similar to GetGrantedObjects() but returns only extended grants.
  std::vector<std::unique_ptr<Object>> GetExtendedPersistedObjects(
      const url::Origin& origin);

  // Revokes the active grants for the given origin, and returns whether any is
  // revoked. If the `file_path` is provided, then only the grant matching
  // the file path is revoked.
  bool RevokeActiveGrants(const url::Origin& origin,
                          base::FilePath file_path = base::FilePath());

  base::WeakPtr<ChromeFileSystemAccessPermissionContext> GetWeakPtr();

  const raw_ptr<content::BrowserContext, DanglingUntriaged> profile_;

  // Permission state per origin.
  struct OriginState;
  std::map<url::Origin, OriginState> active_permissions_map_;

  bool usage_icon_update_scheduled_ = false;

  scoped_refptr<HostContentSettingsMap> content_settings_;

  base::ScopedObservation<OneTimePermissionsTracker,
                          OneTimePermissionsTrackerObserver>
      one_time_permissions_tracker_{this};
#if !BUILDFLAG(IS_ANDROID)
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};
#endif

  // Number of custom IDs an origin can specify.
  size_t max_ids_per_origin_ = 32u;

  const raw_ptr<const base::Clock> clock_;

  // Subscribers to notify of file creation events originating from
  // `window.showSaveFilePicker()`.
  FileCreatedFromShowSaveFilePickerCallbackList
      file_created_from_show_save_file_picker_callback_list_;

  base::WeakPtrFactory<ChromeFileSystemAccessPermissionContext> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_CHROME_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
