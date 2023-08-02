// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_EXTERNAL_UPDATER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_EXTERNAL_UPDATER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/app_mode/kiosk_external_update_validator.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"

namespace ash {

class KioskExternalUpdateNotification;

// Observes the disk mount/unmount events, scans the usb stick for external
// kiosk app updates, validates the external crx, and updates the cache.
class KioskExternalUpdater : public disks::DiskMountManager::Observer,
                             public KioskExternalUpdateValidatorDelegate {
 public:
  enum class ErrorCode {
    kNone,
    kNoManifest,
    kInvalidManifest,
  };

  KioskExternalUpdater(
      const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
      const base::FilePath& crx_cache_dir,
      const base::FilePath& crx_unpack_dir);
  KioskExternalUpdater(const KioskExternalUpdater&) = delete;
  KioskExternalUpdater& operator=(const KioskExternalUpdater&) = delete;
  ~KioskExternalUpdater() override;

 private:
  enum class UpdateStatus {
    kPending,
    kSuccess,
    kFailed,
  };
  struct ExternalUpdate {
    ExternalUpdate();
    ExternalUpdate(const ExternalUpdate& other);
    ~ExternalUpdate();

    std::string app_name;
    extensions::CRXFileInfo external_crx;
    UpdateStatus update_status;
    std::u16string error;
  };

  // disks::DiskMountManager::Observer overrides.
  void OnMountEvent(
      disks::DiskMountManager::MountEvent event,
      MountError error_code,
      const disks::DiskMountManager::MountPoint& mount_info) override;

  // KioskExternalUpdateValidatorDelegate overrides:
  void OnExternalUpdateUnpackSuccess(const std::string& app_id,
                                     const std::string& version,
                                     const std::string& min_browser_version,
                                     const base::FilePath& temp_dir) override;
  void OnExternalUpdateUnpackFailure(const std::string& app_id) override;

  // Processes the parsed external update manifest, check the ErrorCode in
  // `result` for any manifest parsing error.
  using ParseManifestResult = std::pair<base::Value, ErrorCode>;
  void ProcessParsedManifest(const base::FilePath& external_update_dir,
                             const ParseManifestResult& result);

  // Returns true if `external_update_` is interrupted before the updating
  // completes.
  bool CheckExternalUpdateInterrupted();

  // Validates the external updates.
  void ValidateExternalUpdates();

  // Returns true if there are any external updates pending.
  bool IsExternalUpdatePending() const;

  // Returns true if all external updates specified in the manifest are
  // completed successfully.
  bool IsAllExternalUpdatesSucceeded() const;

  // Returns true if the app with `app_id` should be updated to
  // `external_extension`.
  bool ShouldDoExternalUpdate(const std::string& app_id,
                              const std::string& version,
                              const std::string& min_browser_version);

  // Installs the validated extension into cache.
  // `crx_copied` indicates whether the `crx_file` is copied successfully.
  void PutValidatedExtension(const std::string& app_id,
                             const base::FilePath& crx_file,
                             const std::string& version,
                             bool crx_copied);

  // Called upon completion of installing the validated external extension into
  // the local cache. `success` is true if the operation succeeded.
  void OnPutValidatedExtension(const std::string& app_id, bool success);

  void NotifyKioskUpdateProgress(const std::u16string& message);

  void MaybeValidateNextExternalUpdate();

  // Notifies the kiosk update status with UI and KioskAppUpdateService, if
  // there is no kiosk external updates pending.
  void MayBeNotifyKioskAppUpdate();

  void NotifyKioskAppUpdateAvailable();

  // Dismisses the UI notification for kiosk updates.
  void DismissKioskUpdateNotification();

  // Return a detailed message for kiosk updating status.
  std::u16string GetUpdateReportMessage() const;

  // Task runner for executing file I/O tasks.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // The directory where kiosk crx files are cached.
  const base::FilePath crx_cache_dir_;

  // The directory used by SandBoxedUnpacker for unpack extensions.
  const base::FilePath crx_unpack_dir_;

  // The path where external crx files resides(usb stick mount path).
  base::FilePath external_update_path_;

  // map of app_id: ExternalUpdate
  using ExternalUpdateMap = std::map<std::string, ExternalUpdate>;
  ExternalUpdateMap external_updates_;

  std::unique_ptr<KioskExternalUpdateNotification> notification_;

  base::WeakPtrFactory<KioskExternalUpdater> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_EXTERNAL_UPDATER_H_
