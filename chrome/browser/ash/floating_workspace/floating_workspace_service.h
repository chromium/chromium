// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_

#include <memory>
#include "ash/public/cpp/desk_template.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/keyed_service/core/keyed_service.h"
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
};

// A keyed service to support floating workspace. Note that a periodical
// task `CaptureAndUploadActiveDesk` will be dispatched during service
// initialization.
class FloatingWorkspaceService : public KeyedService,
                                 public message_center::NotificationObserver,
                                 public syncer::SyncServiceObserver {
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

  // message_center::NotificationObserver overrides:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

  void MaybeCloseNotification();

 protected:
  std::unique_ptr<DeskTemplate> previously_captured_desk_template_;
  // Indicate if it is a testing class.
  bool is_testing_ = false;

 private:
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

  // Get latest Floating Workspace Template from DeskSyncBridge.
  const DeskTemplate* GetLatestFloatingWorkspaceTemplate();

  // Capture the current active desk task, running every ~30(TBD) seconds.
  // Upload captured desk to chrome sync and record the randomly generated
  // UUID key to `floating_workspace_template_uuid_`.
  void CaptureAndUploadActiveDesk();

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

  // Handles the recording of the error for template launch.
  void HandleTemplateUploadErrors(DesksClient::DeskActionError error);

  // Callback function that is run after a floating workspace template
  // is downloaded and launched.
  void OnTemplateLaunched(absl::optional<DesksClient::DeskActionError> error,
                          const base::Uuid& desk_uuid);

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

  const raw_ptr<Profile, ExperimentalAsh> profile_;

  const floating_workspace_util::FloatingWorkspaceVersion version_;

  raw_ptr<sync_sessions::SessionSyncService, ExperimentalAsh>
      session_sync_service_;

  base::CallbackListSubscription foreign_session_updated_subscription_;

  // Flag to determine if we should run the restore.
  bool should_run_restore_ = true;

  // Time when the service is initialized.
  base::TimeTicks initialization_timestamp_;

  // Timer used for periodic capturing and uploading.
  base::RepeatingTimer timer_;

  // Timer used to wait for internet connection after service initialization.
  base::OneShotTimer connection_timer_;

  // Convenience pointer to desks_storage::DeskSyncService. Guaranteed to be not
  // null for the duration of `this`.
  raw_ptr<desks_storage::DeskSyncService> desk_sync_service_ = nullptr;

  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // The uuid associated with this device's floating workspace template. This is
  // populated when we first capture a floating workspace template.
  absl::optional<base::Uuid> floating_workspace_uuid_;

  std::unique_ptr<message_center::Notification> notification_;

  // Weak pointer factory used to provide references to this service.
  base::WeakPtrFactory<FloatingWorkspaceService> weak_pointer_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_
