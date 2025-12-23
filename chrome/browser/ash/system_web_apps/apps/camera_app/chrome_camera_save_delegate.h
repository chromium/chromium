// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CHROME_CAMERA_SAVE_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CHROME_CAMERA_SAVE_DELEGATE_H_

#include <cstdint>
#include <variant>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chromeos/ash/experiences/camera/camera_save_handler.h"
#include "storage/browser/file_system/file_system_url.h"

namespace content {
class BrowserContext;
}

// Chrome delegate for camera save operations. There is an instance
// of this class per profile.
class ChromeCameraSaveDelegate : public CameraSaveHandler::Delegate {
 public:
  explicit ChromeCameraSaveDelegate(content::BrowserContext* context);

  ChromeCameraSaveDelegate(const ChromeCameraSaveDelegate&) = delete;
  ChromeCameraSaveDelegate& operator=(const ChromeCameraSaveDelegate&) = delete;

  ~ChromeCameraSaveDelegate() override;

 private:
  // CameraSaveHandler::Delegate implementation.
  CameraSaveHandler::FileSaveDestination GetDestination() const override;
  base::FilePath GetMyFilesFolder() const override;
  base::FilePath GetOneDriveUploadFolder() const override;
  void PerformUpload(const base::FilePath& upload_from_path,
                     int64_t file_size,
                     const gfx::Image& thumbnail,
                     base::RepeatingCallback<void(int64_t)> progress_callback,
                     base::OnceCallback<void(bool)> done_callback) override;

  bool is_onedrive() const {
    return destination_ ==
           policy::local_user_files::FileSaveDestination::kOneDrive;
  }
  bool is_google_drive() const {
    return destination_ ==
           policy::local_user_files::FileSaveDestination::kGoogleDrive;
  }

  std::string GetPathFromPref() const;
  base::FilePath GetFinalPathRelativeToRoot() const;
  void CancelUploads(base::OnceClosure cancel_closure);
  void OnOnedriveUploadDone(
      const std::string& file_name,
      base::OnceCallback<void(bool)> callback,
      storage::FileSystemURL,
      std::optional<ash::cloud_upload::OdfsSkyvaultUploader::UploadError>,
      base::FilePath);

  const raw_ptr<content::BrowserContext> context_;
  const policy::local_user_files::FileSaveDestination destination_;
  std::map<std::string, base::WeakPtr<ash::cloud_upload::OdfsSkyvaultUploader>>
      onedrive_uploaders_;
  base::WeakPtrFactory<ChromeCameraSaveDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CHROME_CAMERA_SAVE_DELEGATE_H_
