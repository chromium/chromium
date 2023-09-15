// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_

#include <memory>
#include <string>
#include "ash/public/cpp/desk_template.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

namespace sync_sessions {
class OpenTabsUIDelegate;
class SessionSyncService;
struct SyncedSession;
}  // namespace sync_sessions

namespace ash {

extern const char kNotificationForNoNetworkConnection[];
extern const char kNotificationForSyncErrorOrTimeOut[];
extern const char kNotificationForRestoreAfterError[];
extern const char kNotificationForProgressStatus[];

// The restore from error notification button index.
enum class RestoreFromErrorNotificationButtonIndex {
  kRestore = 0,
  kCancel,
};

// The notification type for floating workspace service.
enum class FloatingWorkspaceServiceNotificationType {
  kUnknown = 0,
  kNoNetworkConnection,
  kSyncErrorOrTimeOut,
  kRestoreAfterError,
  kProgressStatus,
};

// A keyed service to support floating workspace. Note that a periodical
// task `CaptureAndUploadActiveDesk` will be dispatched during service
// initialization.
class FloatingWorkspaceService
    : public KeyedService,
      public message_center::NotificationObserver,
      public syncer::SyncServiceObserver,
      public apps::AppRegistryCache::Observer,
      public apps::AppRegistryCacheWrapper::Observer {
 public:
  static FloatingWorkspaceService* GetForProfile(Profile* profile);

  explicit FloatingWorkspaceService(
      Profile* profile,
      floating_workspace_util::FloatingWorkspaceVersion version);

  ~FloatingWorkspaceService() override;

  // Used in constructor for initializations
  void Init(syncer::SyncService* sync_service,
            desks_storage::DeskSyncService* desk_sync_service);

  // Add subscription to foreign session changes.
  void SubscribeToForeignSessionUpdates();

  // Get and restore most recently used device browser session
  // remote or local.
  void RestoreBrowserWindowsFromMostRecentlyUsedDevice();

  void TryRestoreMostRecentlyUsedSession();

  void CaptureAndUploadActiveDeskForTest(
      std::unique_ptr<DeskTemplate> desk_template);

  // syncer::SyncServiceObserver overrides:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // message_center::NotificationObserver overrides:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

  void MaybeCloseNotification();

  std::vector<const ash::DeskTemplate*> GetFloatingWorkspaceTemplateEntries();

 protected:
  std::unique_ptr<DeskTemplate> previously_captured_desk_template_;
  // Indicate if it is a testing class.
  bool is_testing_ = false;

 private:
  // AppRegistryCache::Observer
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;
  void OnAppTypeInitialized(apps::AppType app_type) override;

  // AppRegistryCacheWrapper::Observer
  void OnAppRegistryCacheAdded(const AccountId& account_id) override;

  void InitForV1();
  void InitForV2(syncer::SyncService* sync_service,
                 desks_storage::DeskSyncService* desk_sync_service);

  const sync_sessions::SyncedSession* GetMostRecentlyUsedRemoteSession();

  const sync_sessions::SyncedSession* GetLocalSession();

  // Virtual for testing.
  virtual void RestoreForeignSessionWindows(
      const sync_sessions::SyncedSession* session);

  // Virtual for testing.
  virtual void RestoreLocalSessionWindows();

  // Virtual for testing.
  virtual sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate();

  // Start and Stop capturing and uploading the active desks.
  void StartCaptureAndUploadActiveDesk();
  void StopCaptureAndUploadActiveDesk();

  // Start and stop the progress bar notification.
  void MaybeStartProgressBarNotification();
  void StopProgressBarNotification();

  // Handles the updating of progress bar notification.
  void HandleProgressBarStatus();

  // Get latest Floating Workspace Template from DeskSyncBridge.
  const DeskTemplate* GetLatestFloatingWorkspaceTemplate();

  // Capture the current active desk task, running every ~30(TBD) seconds.
  // Upload captured desk to chrome sync and record the randomly generated
  // UUID key to `floating_workspace_template_uuid_`.
  void CaptureAndUploadActiveDesk();

  // Stops the progress bar and resumes the latest floating workspace. This is
  // called when the app cache is ready and we have received `kUpToDate` from
  // sync service.
  void StopProgressBarAndRestoreFloatingWorkspace();

  // Restore last saved floating workspace desk for current user with
  // floating_workspace_template_uuid_.
  void RestoreFloatingWorkspaceTemplate(const DeskTemplate* desk_template);

  // Launch downloaded floating workspace desk when all conditions are met.
  // Virtual for testing.
  virtual void LaunchFloatingWorkspaceTemplate(
      const DeskTemplate* desk_template);

  // Return the desk client to be used, in test it will return a mocked one.
  virtual DesksClient* GetDesksClient();

  // Compare currently captured and previous floating workspace desk.
  // Called by CaptureAndUploadActiveDesk before upload.
  // If no difference is recorded no upload job will be triggered.
  bool IsCurrentDeskSameAsPrevious(DeskTemplate* current_desk_template) const;

  // Handles the recording of the error for template capture.
  void HandleTemplateCaptureErrors(DesksClient::DeskActionError error);

  // Callback function that is run after a floating workspace template is
  // captured by `desks_storage::DeskSyncBridge`.
  void OnTemplateCaptured(absl::optional<DesksClient::DeskActionError> error,
                          std::unique_ptr<DeskTemplate> desk_template);

  // Upload floating workspace desk template after detecting that it's a
  // different template. Virtual for testing.
  virtual void UploadFloatingWorkspaceTemplateToDeskModel(
      std::unique_ptr<DeskTemplate> desk_template);

  void OnTemplateUploaded(
      desks_storage::DeskModel::AddOrUpdateEntryStatus status,
      std::unique_ptr<DeskTemplate> new_entry);

  // Get the associated floating workspace uuid for the current device. Return
  // an absl::nullopt if there is no floating workspace uuid that is associated
  // with the current device.
  absl::optional<base::Uuid> GetFloatingWorkspaceUuidForCurrentDevice();
  // When sync passes an error status to floating workspace service,
  // floating workspace service should send notification to user asking whether
  // to restore the most recent FWS desk from local storage.
  void HandleSyncEror();

  // When floating workspace service waited long enough but no desk is restored
  // floating workspace service should send notification to user asking whether
  // to restore the most recent FWS desk from local storage.
  void MaybeHandleDownloadTimeOut();

  void SendNotification(const std::string& id);

  // Performs garbage collection of stale floating workspace templates. A
  // floating workspace template is considered stale if it's older than 30
  // days. The only exception is if it's the only floating workspace
  // template associated with the current user, which we want to keep.
  void DoGarbageCollection(const DeskTemplate* exclude_template);

  // Close desks that were already open on current device.
  void RemoveAllPreviousDesksExceptActiveDesk(
      const base::Uuid& exclude_desk_uuid);

  // Sign out of the current user session when we detect another active session
  // after this service was started.
  void MaybeSignOutOfCurrentSession();

  // Updates the `is_cache_ready_` status if all the required app types are
  // initialized.
  bool AreRequiredAppTypesInitialized();

  const raw_ptr<Profile, ExperimentalAsh> profile_;

  const floating_workspace_util::FloatingWorkspaceVersion version_;

  raw_ptr<sync_sessions::SessionSyncService, ExperimentalAsh>
      session_sync_service_;

  base::CallbackListSubscription foreign_session_updated_subscription_;

  // Flag to determine if we should run the restore.
  bool should_run_restore_ = true;

  // Tells us whether or not the apps cache is ready.
  bool is_cache_ready_ = false;

  // Flag to tell us if we should launch on cache is ready.
  bool should_launch_on_ready_ = false;

  // Time when the service is initialized.
  base::TimeTicks initialization_timeticks_;

  // Time when service is initialized in base::Time format for comparison with
  // desk template time.
  base::Time initialization_time_;

  // Time when we first received `kUpToDate` status from `sync_service_`
  absl::optional<base::TimeTicks> first_uptodate_download_timeticks_;

  // Timer used for periodic capturing and uploading.
  base::RepeatingTimer timer_;

  // Timer used to wait for internet connection after service initialization.
  base::OneShotTimer connection_timer_;

  // Timer used to periodically update the progress status bar based on time
  // from the 2 seconds after login to 15 seconds max wait time.
  base::RepeatingTimer progress_timer_;

  // Convenience pointer to desks_storage::DeskSyncService. Guaranteed to be not
  // null for the duration of `this`.
  raw_ptr<desks_storage::DeskSyncService> desk_sync_service_ = nullptr;

  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // The uuid associated with this device's floating workspace template. This is
  // populated when we first capture a floating workspace template.
  absl::optional<base::Uuid> floating_workspace_uuid_;

  std::unique_ptr<message_center::Notification> notification_;
  std::string progress_notification_id_;

  // The in memory cache of the floating workspace that should be restored after
  // downloading latest updates. Saved in case the user delays resuming the
  // session and a captured template was uploaded.
  std::unique_ptr<DeskTemplate> floating_workspace_template_to_restore_ =
      nullptr;

  // scoped Observations
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_cache_obs_{this};
  base::ScopedObservation<apps::AppRegistryCacheWrapper,
                          apps::AppRegistryCacheWrapper::Observer>
      app_cache_wrapper_obs_{this};
  // Weak pointer factory used to provide references to this service.
  base::WeakPtrFactory<FloatingWorkspaceService> weak_pointer_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_
