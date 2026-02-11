// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/camera_app/chrome_camera_save_delegate.h"

#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/skyvault/drive_upload_observer.h"
#include "chrome/browser/ash/policy/skyvault/file_location_utils.h"
#include "chrome/browser/ash/policy/skyvault/odfs_file_deleter.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chromeos/ash/experiences/camera/camera_save_handler.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace {

const std::string& GetPathFromPref(content::BrowserContext* context) {
  auto* prefs = Profile::FromBrowserContext(context)->GetPrefs();
  CHECK(prefs);
  const auto* pref = prefs->FindPreference(ash::prefs::kCameraSaveLocation);
  return pref->GetValue()->GetString();
}

}  // namespace

ChromeCameraSaveDelegate::ChromeCameraSaveDelegate(
    content::BrowserContext* context)
    : context_(context),
      destination_(policy::local_user_files::GetCameraDestination(
          Profile::FromBrowserContext(context_))),
      path_from_pref_(GetPathFromPref(context_)) {}

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

base::FilePath ChromeCameraSaveDelegate::GetFinalPathRelativeToRoot() const {
  // The first component is expected to be the root folder variable, e.g.
  // "${microsoft_onedrive}".
  auto components = base::FilePath(path_from_pref_).GetComponents();

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
    base::OnceCallback<void(bool, std::optional<base::FilePath>)>
        done_callback) {
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
    google_drive_uploaders_.emplace(
        file_name,
        ash::cloud_upload::DriveUploadObserver::Observe(
            profile, upload_from_path,
            policy::local_user_files::UploadTrigger::kCamera, file_size,
            progress_callback,
            base::BindOnce(&ChromeCameraSaveDelegate::OnGoogleDriveUploadDone,
                           weak_ptr_factory_.GetWeakPtr(), file_name,
                           std::move(done_callback))));
  }
}

void ChromeCameraSaveDelegate::CancelUploads() {
  // When Cancel() is called, OnOnedriveUploadDone() / OnGoogleDriveUploadDone()
  // will be called which would modify `onedrive_uploaders_` or
  // `google_drive_uploaders_`. So make a copy of the uploaders, and explicitly
  // clear the map here to control cancellation, and iterate over the uploaders
  // instead.
  if (is_onedrive()) {
    std::vector<base::WeakPtr<ash::cloud_upload::OdfsSkyvaultUploader>>
        uploaders;
    uploaders.reserve(onedrive_uploaders_.size());
    for (auto& [_, uploader] : onedrive_uploaders_) {
      uploaders.push_back(uploader);
    }
    // Clear map before OnOnedriveUploadDone() can be called to indicate
    // uploads have been cancelled, so that it doesn't call the done callbacks.
    onedrive_uploaders_.clear();
    for (auto& uploader : uploaders) {
      if (uploader) {
        uploader->Cancel();
      }
    }
  } else {
    CHECK(is_google_drive());
    std::vector<base::WeakPtr<ash::cloud_upload::DriveUploadObserver>>
        uploaders;
    uploaders.reserve(google_drive_uploaders_.size());
    for (auto& [_, uploader] : google_drive_uploaders_) {
      uploaders.push_back(uploader);
    }
    // Clear map before OnGoogleDriveUploadDone() can be called to indicate
    // uploads have been cancelled, so that it doesn't call the done callbacks.
    google_drive_uploaders_.clear();
    for (auto& uploader : uploaders) {
      if (uploader) {
        uploader->Cancel();
      }
    }
  }
}

void ChromeCameraSaveDelegate::OpenFileInImageEditor(
    const base::FilePath& file_path) {
  ash::SystemAppLaunchParams params;
  params.launch_paths = {file_path};
  params.launch_source = apps::LaunchSource::kFromFileManager;
  ash::LaunchSystemWebAppAsync(Profile::FromBrowserContext(context_),
                               ash::SystemWebAppType::MEDIA, params);
}

void ChromeCameraSaveDelegate::DeleteFileOnOneDrive(
    const base::FilePath& file_path,
    base::OnceCallback<void(bool)> callback) {
  ash::cloud_upload::OdfsFileDeleter::Delete(file_path, std::move(callback));
}

void ChromeCameraSaveDelegate::OpenCameraApp() {
  ash::LaunchSystemWebAppAsync(Profile::FromBrowserContext(context_),
                               ash::SystemWebAppType::CAMERA);
}

void ChromeCameraSaveDelegate::OnOnedriveUploadDone(
    const std::string& file_name,
    base::OnceCallback<void(bool, std::optional<base::FilePath>)> callback,
    storage::FileSystemURL uploaded_file_url,
    std::optional<ash::cloud_upload::OdfsSkyvaultUploader::UploadError> error,
    base::FilePath /*upload_root_path*/) {
  if (!onedrive_uploaders_.erase(file_name)) {
    // Uploads have been cancelled by the user. So don't invoke the done
    // callback.
    return;
  }
  std::move(callback).Run(!error, uploaded_file_url.path() /* uploaded_path */);
}

void ChromeCameraSaveDelegate::OnGoogleDriveUploadDone(
    const std::string& file_name,
    base::OnceCallback<void(bool, std::optional<base::FilePath>)> callback,
    bool success) {
  if (!google_drive_uploaders_.erase(file_name)) {
    // Uploads have been cancelled by the user. So don't invoke the done
    // callback.
    return;
  }
  std::move(callback).Run(success, std::nullopt /* uploaded_path */);
}
