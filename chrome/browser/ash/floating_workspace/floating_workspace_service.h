// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_

#include "ash/public/cpp/desk_template.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace sync_sessions {
class OpenTabsUIDelegate;
class SessionSyncService;
struct SyncedSession;
}  // namespace sync_sessions

namespace desks_storage {
class DeskModelObserver;
}  // namespace desks_storage

namespace ash {

// Testing enum to substitute for feature flag during testing.
enum TestFloatingWorkspaceVersion {
  // Default value, indicates no version was enabled.
  kNoVersionEnabled = 0,

  // Version 1.
  kFloatingWorkspaceV1Enabled = 1,

  // Version 2.
  kFloatingWorkspaceV2Enabled = 2,
};

// A keyed service to support floating workspace. Note that a periodical
// task `CaptureAndUploadActiveDesk` will be dispatched during service
// initialization.
class FloatingWorkspaceService : public KeyedService,
                                 public desks_storage::DeskModelObserver {
 public:
  static FloatingWorkspaceService* GetForProfile(Profile* profile);

  explicit FloatingWorkspaceService(Profile* profile);

  ~FloatingWorkspaceService() override;

  // Used in constructor for initializations
  void Init();
  void InitForTest(TestFloatingWorkspaceVersion version);

  // Add subscription to foreign session changes.
  void SubscribeToForeignSessionUpdates();

  // Get and restore most recently used device browser session
  // remote or local.
  void RestoreBrowserWindowsFromMostRecentlyUsedDevice();

  void TryRestoreMostRecentlyUsedSession();

  // desks_storage::DeskModelObserver overrides:
  void DeskModelLoaded() override {}
  void OnDeskModelDestroying() override;
  void EntriesAddedOrUpdatedRemotely(
      const std::vector<const DeskTemplate*>& new_entries) override;
  void EntriesRemovedRemotely(const std::vector<base::Uuid>& uuids) override {}

 private:
  void InitForV1();
  void InitForV2();

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
                          std::unique_ptr<DeskTemplate>);

  void OnTemplateUploaded(
      desks_storage::DeskModel::AddOrUpdateEntryStatus status,
      std::unique_ptr<DeskTemplate> new_entry);

  const raw_ptr<Profile, ExperimentalAsh> profile_;

  raw_ptr<sync_sessions::SessionSyncService, ExperimentalAsh>
      session_sync_service_;

  base::CallbackListSubscription foreign_session_updated_subscription_;

  // Flag to determine if we should run the restore.
  bool should_run_restore_ = true;

  // Time when the service is initialized.
  base::TimeTicks initialization_timestamp_;

  // Timer used for periodic capturing and uploading.
  base::RepeatingTimer timer_;

  // Convenience pointer to desks_storage::DeskSyncService. Guaranteed to be not
  // null for the duration of `this`.
  raw_ptr<desks_storage::DeskSyncService> desk_sync_service_ = nullptr;

  std::unique_ptr<DeskTemplate> previously_captured_desk_template_;

  // Indicate if it is a testing class.
  bool is_testing_ = false;

  // Weak pointer factory used to provide references to this service.
  base::WeakPtrFactory<FloatingWorkspaceService> weak_pointer_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_
