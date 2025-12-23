// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/camera_app/chrome_camera_save_delegate.h"

#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/skyvault/drive_upload_observer.h"
#include "chrome/browser/ash/policy/skyvault/file_location_utils.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/experiences/camera/camera_save_handler.h"
#include "components/prefs/pref_service.h"

ChromeCameraSaveDelegate::ChromeCameraSaveDelegate(
    content::BrowserContext* context)
    : context_(context),
      destination_(policy::local_user_files::GetCameraDestination(
          Profile::FromBrowserContext(context_))) {}

ChromeCameraSaveDelegate::~ChromeCameraSaveDelegate() = default;

CameraSaveHandler::FileSaveDestination
ChromeCameraSaveDelegate::GetDestination() const {
  switch (destination_) {
    case policy::local_user_files::FileSaveDestination::kOneDrive:
      return CameraSaveHandler::FileSaveDestination::kOneDrive;
    case policy::local_user_files::FileSaveDestination::kGoogleDrive:
      return CameraSaveHandler::FileSaveDestination::kGoogleDrive;
    case policy::local_user_files::FileSaveDestination::kNotSpecified:
    case policy::local_user_files::FileSaveDestination::kDownloads:
      return CameraSaveHandler::FileSaveDestination::kLocal;
  }
}

base::FilePath ChromeCameraSaveDelegate::GetMyFilesFolder() const {
  return file_manager::util::GetMyFilesFolderForProfile(
      Profile::FromBrowserContext(context_));
}

base::FilePath ChromeCameraSaveDelegate::GetGoogleDriveRoot() const {
  return policy::local_user_files::GetGoogleDriveRoot();
}

std::string ChromeCameraSaveDelegate::GetPathFromPref() const {
  auto* prefs = Profile::FromBrowserContext(context_)->GetPrefs();
  CHECK(prefs);
  const auto* pref = prefs->FindPreference(ash::prefs::kCameraSaveLocation);
  return pref->GetValue()->GetString();
}

base::FilePath ChromeCameraSaveDelegate::GetFinalPathRelativeToRoot() const {
  // The first component is expected to be the root folder variable, e.g.
  // "${microsoft_onedrive}".
  auto components = base::FilePath(GetPathFromPref()).GetComponents();

  if (components.size() <= 1) {
    // No subfolder specified.
    return base::FilePath();
  }

  // Return the first subfolder component.
  // Only one level of nested subfolders is currently supported.
  return base::FilePath(components[1]);
}

base::FilePath ChromeCameraSaveDelegate::GetOneDriveUploadFolder() const {
  DCHECK(is_onedrive());
  Profile* profile = Profile::FromBrowserContext(context_);
  if (!ash::cloud_upload::IsODFSMounted(profile)) {
    LOG(ERROR) << "ODFS is not mounted.";
    return base::FilePath();
  }
  return ash::cloud_upload::GetODFSFuseboxMount(profile).Append(
      GetFinalPathRelativeToRoot());
}

void ChromeCameraSaveDelegate::PerformUpload(
    const base::FilePath& upload_from_path,
    int64_t file_size,
    const gfx::Image& thumbnail,
    base::RepeatingCallback<void(int64_t)> progress_callback,
    base::OnceCallback<void(bool)> done_callback) {
  CHECK(is_onedrive() || is_google_drive());
  Profile* profile = Profile::FromBrowserContext(context_);
  std::string file_name = upload_from_path.BaseName().AsUTF8Unsafe();
  if (is_onedrive()) {
    CHECK(
        onedrive_uploaders_
            .emplace(file_name,
                     ash::cloud_upload::OdfsSkyvaultUploader::Upload(
                         profile, upload_from_path,
                         /*relative_source_path=*/GetFinalPathRelativeToRoot(),
                         /*upload_root=*/{},
                         policy::local_user_files::UploadTrigger::kCamera,
                         progress_callback,
                         base::BindOnce(
                             &ChromeCameraSaveDelegate::OnOnedriveUploadDone,
                             weak_ptr_factory_.GetWeakPtr(), file_name,
                             std::move(done_callback)),
                         thumbnail))
            .second);
  } else {
    ash::cloud_upload::DriveUploadObserver::Observe(
        profile, upload_from_path,
        policy::local_user_files::UploadTrigger::kCamera, file_size,
        progress_callback, std::move(done_callback));
  }
}

void ChromeCameraSaveDelegate::OnOnedriveUploadDone(
    const std::string& file_name,
    base::OnceCallback<void(bool)> callback,
    storage::FileSystemURL,
    std::optional<ash::cloud_upload::OdfsSkyvaultUploader::UploadError> error,
    base::FilePath /*upload_root_path*/) {
  onedrive_uploaders_.erase(file_name);
  std::move(callback).Run(!error);
}
