// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ash/components/drivefs/drivefs_host_observer.h"
#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace extensions {
namespace api {
namespace file_manager_private {
struct FileTransferStatus;
struct FileWatchEvent;
}  // namespace file_manager_private
}  // namespace api
}  // namespace extensions

namespace file_manager {

// Files app's event router handling DriveFS-related events.
class DriveFsEventRouter : public drivefs::DriveFsHostObserver {
 public:
  explicit DriveFsEventRouter(SystemNotificationManager* notification_manager);
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

 protected:
  SystemNotificationManager* system_notification_manager() {
    return notification_manager_;
  }

 private:
  struct SyncingStatusState {
    SyncingStatusState();
    ~SyncingStatusState();

    std::map<int64_t, int64_t> group_id_to_bytes_to_transfer;
    int64_t completed_bytes = 0;
  };

  // DriveFsHostObserver:
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnFilesChanged(
      const std::vector<drivefs::mojom::FileChange>& changes) override;
  void OnError(const drivefs::mojom::DriveError& error) override;

  virtual std::set<GURL> GetEventListenerURLs(
      const std::string& event_name) = 0;

  virtual GURL ConvertDrivePathToFileSystemUrl(const base::FilePath& file_path,
                                               const GURL& listener_url) = 0;

  virtual std::string GetDriveFileSystemName() = 0;

  virtual bool IsPathWatched(const base::FilePath& path) = 0;

  void BroadcastOnFileTransfersUpdatedEvent(
      const extensions::api::file_manager_private::FileTransferStatus& status);

  void BroadcastOnPinTransfersUpdatedEvent(
      const extensions::api::file_manager_private::FileTransferStatus& status);

  void BroadcastOnDirectoryChangedEvent(
      const base::FilePath& directory,
      const extensions::api::file_manager_private::FileWatchEvent& event);

  // Helper method for broadcasting events.
  virtual void BroadcastEvent(
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      base::Value::List event_args) = 0;

  static extensions::api::file_manager_private::FileTransferStatus
  CreateFileTransferStatus(
      const std::vector<drivefs::mojom::ItemEvent*>& item_events,
      SyncingStatusState* state);

  // This is owned by EventRouter and only shared with this class.
  SystemNotificationManager* notification_manager_;

  SyncingStatusState sync_status_state_;
  SyncingStatusState pin_status_state_;
  base::OnceCallback<void(drivefs::mojom::DialogResult)> dialog_callback_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_
