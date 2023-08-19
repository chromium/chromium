// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/extensions/file_manager/system_notification_manager.h"
#include "chromeos/ash/components/drivefs/drivefs_host_observer.h"
#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/drivefs/sync_status_tracker.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace extensions {
namespace api {
namespace file_manager_private {
struct FileTransferStatus;
struct IndividualFileTransferStatus;
struct FileWatchEvent;
}  // namespace file_manager_private
}  // namespace api
}  // namespace extensions

namespace file_manager {

using extensions::api::file_manager_private::FileTransferStatus;
using IndividualFileTransferStatus =
    extensions::api::file_manager_private::SyncState;

// Files app's event router handling DriveFS-related events.
class DriveFsEventRouter : public drivefs::DriveFsHostObserver,
                           public drive::DriveIntegrationServiceObserver {
 public:
  DriveFsEventRouter(Profile* profile,
                     SystemNotificationManager* notification_manager);
  DriveFsEventRouter(const DriveFsEventRouter&) = delete;
  ~DriveFsEventRouter() override;

  DriveFsEventRouter& operator=(const DriveFsEventRouter&) = delete;

  // Triggers an event in the UI to display a confirmation dialog.
  void DisplayConfirmDialog(
      const drivefs::mojom::DialogReason& reason,
      base::OnceCallback<void(drivefs::mojom::DialogResult)> callback);

  // Called from the UI to notify the caller of DisplayConfirmDialog() of the
  // dialog's result.
  void OnDialogResult(drivefs::mojom::DialogResult result);

  // In some cases, we might want to disable Drive notifications for a file
  // identified by its relative Drive path. These methods help control when to
  // suppress and restore these notifications.
  void SuppressNotificationsForFilePath(const base::FilePath& path);
  void RestoreNotificationsForFilePath(const base::FilePath& path);

  drivefs::SyncState GetDriveSyncStateForPath(const base::FilePath& drive_path);

 protected:
  SystemNotificationManager* system_notification_manager() {
    return notification_manager_;
  }

 private:
  struct SyncingStatusState {
    SyncingStatusState();
    SyncingStatusState(const SyncingStatusState& other);
    ~SyncingStatusState();

    std::unordered_map<int64_t, int64_t> group_id_to_bytes_to_transfer;
    int64_t completed_bytes = 0;

    // A given queued item that was received in one batch of events might not
    // necessarily appear in the next batch. Because of that, we keep track of
    // queued bytes and the total of queued items in the state.
    std::unordered_map<int64_t, int64_t> group_id_to_queued_bytes;
    int64_t queued_bytes = 0;
  };

  // DriveFsHostObserver:
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnIndividualSyncingStatusesDelta(
      const std::vector<const drivefs::SyncState>& sync_states) override;
  void OnFilesChanged(
      const std::vector<drivefs::mojom::FileChange>& changes) override;
  void OnError(const drivefs::mojom::DriveError& error) override;
  void OnItemProgress(const drivefs::mojom::ProgressEvent& event) override;

  // DriveIntegrationServiceObserver:
  void OnBulkPinProgress(const drivefs::pinning::Progress& progress) override;

  virtual std::set<GURL> GetEventListenerURLs(
      const std::string& event_name) = 0;

  virtual GURL ConvertDrivePathToFileSystemUrl(const base::FilePath& file_path,
                                               const GURL& listener_url) = 0;

  virtual std::vector<GURL> ConvertPathsToFileSystemUrls(
      const std::vector<base::FilePath>& paths,
      const GURL& listener_url) = 0;

  virtual std::string GetDriveFileSystemName() = 0;

  virtual bool IsPathWatched(const base::FilePath& path) = 0;

  void BroadcastTransferEvent(
      const extensions::events::HistogramValue event_type,
      const FileTransferStatus& status);

  void BroadcastIndividualTransfersEvent(
      const extensions::events::HistogramValue event_type,
      const std::vector<IndividualFileTransferStatus>& status);

  void BroadcastOnDirectoryChangedEvent(
      const base::FilePath& directory,
      const extensions::api::file_manager_private::FileWatchEvent& event);

  // Helper method for broadcasting events.
  virtual void BroadcastEvent(
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      base::Value::List event_args,
      bool dispatch_to_system_notification = true) = 0;

  // Send single event with aggregate sync information for all ItemEvents.
  // Note: this assumes all ItemEvents have the same `reason`.
  void BroadcastAggregateTransferEventForItems(
      const std::vector<const drivefs::mojom::ItemEvent*>& items,
      const extensions::events::HistogramValue& event_type,
      const std::string& event_name,
      SyncingStatusState& state);

  // Send single event with array of per-file sync information for all
  // ItemEvents. Note: this assumes all ItemEvents have the same `reason`.
  void BroadcastIndividualTransferEventsForItems(
      const std::vector<const drivefs::mojom::ItemEvent*>& items,
      const extensions::events::HistogramValue& event_type,
      const std::string& event_name);

  // This is owned by EventRouter and only shared with this class.
  raw_ptr<SystemNotificationManager, ExperimentalAsh> notification_manager_;

  SyncingStatusState sync_status_state_;
  SyncingStatusState pin_status_state_;
  // Set of paths for which Drive transfer events are ignored.
  std::set<base::FilePath> ignored_file_paths_;
  base::OnceCallback<void(drivefs::mojom::DialogResult)> dialog_callback_;

  std::map<std::string, drivefs::SyncState> path_to_sync_state_;
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<DriveFsEventRouter> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_
