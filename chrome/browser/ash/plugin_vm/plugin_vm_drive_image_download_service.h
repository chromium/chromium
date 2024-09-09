// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_DRIVE_IMAGE_DOWNLOAD_SERVICE_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_DRIVE_IMAGE_DOWNLOAD_SERVICE_H_

// We're supposed to use base/integral_types.h per the style guide but stdint.h
// is what DriveAPIService uses.
#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer.h"
#include "crypto/secure_hash.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/drive/drive_common_callbacks.h"

namespace drive {
class DriveServiceInterface;
}  // namespace drive

namespace base {
class FilePath;
}  // namespace base

class Profile;

namespace plugin_vm {

extern const char kPluginVmDriveDownloadDirectory[];

class PluginVmDriveImageDownloadService {
 public:
  using OnDownloadStartedCallback = base::OnceCallback<void()>;
  using OnDownloadFailedCallback =
      base::OnceCallback<void(PluginVmInstaller::FailureReason)>;
  using OnFileDeletedCallback = base::OnceCallback<void(bool)>;

  PluginVmDriveImageDownloadService(PluginVmInstaller* plugin_vm_installer,
                                    Profile* profile);
  PluginVmDriveImageDownloadService(const PluginVmDriveImageDownloadService&) =
      delete;
  PluginVmDriveImageDownloadService& operator=(
      const PluginVmDriveImageDownloadService&) = delete;
  ~PluginVmDriveImageDownloadService();

  void StartDownload(const std::string& id);
  void CancelDownload();

  // Used to reset the internal state of the downloader.
  void ResetState();

  // Removes the temporary image archive and the containing folder
  // on a non-UI thread.
  void RemoveTemporaryArchive(OnFileDeletedCallback on_file_deleted_callback);

  void SetDriveServiceForTesting(
      std::unique_ptr<drive::DriveServiceInterface> drive_service);
  void SetDownloadDirectoryForTesting(const base::FilePath& download_directory);

 private:
  void DispatchDownloadFile();

  void DownloadActionCallback(google_apis::ApiErrorCode error_code,
                              const base::FilePath& file_path);
  void GetContentCallback(google_apis::ApiErrorCode error_code,
                          std::unique_ptr<std::string> content,
                          bool first_chunk);
  void ProgressCallback(int64_t progress, int64_t total);

  raw_ptr<PluginVmInstaller> plugin_vm_installer_;
  std::unique_ptr<drive::DriveServiceInterface> drive_service_;
  std::unique_ptr<crypto::SecureHash> hasher_;
  std::string file_id_;
  int64_t total_bytes_downloaded_ = 0;
  base::FilePath download_directory_{kPluginVmDriveDownloadDirectory};
  base::FilePath download_file_path_;
  google_apis::CancelCallbackOnce cancel_callback_;

  base::WeakPtrFactory<PluginVmDriveImageDownloadService> weak_ptr_factory_{
      this};
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_DRIVE_IMAGE_DOWNLOAD_SERVICE_H_
