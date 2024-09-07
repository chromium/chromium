// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utility functions for fileManagerPrivate API.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_UTIL_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_UTIL_H_

#include <memory>
#include <vector>

#include "base/files/file_error_or.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "components/drive/file_errors.h"
#include "storage/browser/file_system/file_system_url.h"

class GURL;
class Profile;

namespace base {
class File;
class FilePath;
}  // namespace base

namespace drive {
class EventLogger;
}

namespace drivefs::pinning {
struct Progress;
}

namespace extensions {
namespace api {
namespace file_manager_private {
struct EntryProperties;
struct IconSet;
struct VolumeMetadata;
struct MountableGuest;
}  // namespace file_manager_private
}  // namespace api
}  // namespace extensions

namespace ui {
struct SelectedFileInfo;
}

namespace file_manager {

class Volume;

namespace util {

class SingleEntryPropertiesGetterForDriveFs {
 public:
  using ResultCallback = base::OnceCallback<void(
      std::unique_ptr<extensions::api::file_manager_private::EntryProperties>
          properties,
      base::File::Error error)>;

  // Creates an instance and starts the process.
  static void Start(const storage::FileSystemURL& file_system_url,
      Profile* const profile,
      ResultCallback callback);

  ~SingleEntryPropertiesGetterForDriveFs();

  SingleEntryPropertiesGetterForDriveFs(
      const SingleEntryPropertiesGetterForDriveFs&) = delete;
  SingleEntryPropertiesGetterForDriveFs& operator=(
      const SingleEntryPropertiesGetterForDriveFs&) = delete;

 private:
  SingleEntryPropertiesGetterForDriveFs(
      const storage::FileSystemURL& file_system_url,
      Profile* const profile,
      ResultCallback callback);
  void StartProcess();
  void OnGetFileInfo(drive::FileError error,
                     drivefs::mojom::FileMetadataPtr metadata);
  void CompleteGetEntryProperties(drive::FileError error);

  // Given parameters.
  ResultCallback callback_;
  const storage::FileSystemURL file_system_url_;
  base::FilePath relative_path_;
  const raw_ptr<Profile> running_profile_;

  // Values used in the process.
  std::unique_ptr<extensions::api::file_manager_private::EntryProperties>
      properties_;

  base::WeakPtrFactory<SingleEntryPropertiesGetterForDriveFs> weak_ptr_factory_{
      this};
};

// Fills out IDL IconSet struct with the provided icon set.
void FillIconSet(extensions::api::file_manager_private::IconSet* output,
                 const ash::file_system_provider::IconSet& input);

// Converts the |volume| to VolumeMetadata to communicate with JavaScript via
// private API.
void VolumeToVolumeMetadata(
    Profile* profile,
    const Volume& volume,
    extensions::api::file_manager_private::VolumeMetadata* volume_metadata);

// Returns the local FilePath associated with |url|. If the file isn't of the
// type FileSystemBackend handles, returns an empty FilePath.
//
// Local paths will look like "/home/chronos/user/MyFiles/Downloads/foo/bar.txt"
// or "/special/drive/foo/bar.txt".
base::FilePath GetLocalPathFromURL(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const GURL& url);

// The callback type is used for GetSelectedFileInfo().
typedef base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)>
    GetSelectedFileInfoCallback;

// Option enum to control how to set the ui::SelectedFileInfo::local_path
// fields in GetSelectedFileInfo() for Drive files.
// NO_LOCAL_PATH_RESOLUTION:
//   Does nothing. Set the Drive path as-is.
// NEED_LOCAL_PATH_FOR_OPENING:
//   Sets the path to a local cache file.
// NEED_LOCAL_PATH_FOR_SAVING:
//   Sets the path to a local cache file. Modification to the file is monitored
//   and automatically synced to the Drive server.
enum GetSelectedFileInfoLocalPathOption {
  NO_LOCAL_PATH_RESOLUTION,
  NEED_LOCAL_PATH_FOR_OPENING,
  NEED_LOCAL_PATH_FOR_SAVING,
};

// Gets the information for |local_paths|.
void GetSelectedFileInfo(Profile* profile,
                         std::vector<base::FilePath> local_paths,
                         GetSelectedFileInfoLocalPathOption local_path_option,
                         GetSelectedFileInfoCallback callback);

// Get event logger to chrome://drive-internals page for the |profile|.
drive::EventLogger* GetLogger(Profile* profile);

std::vector<extensions::api::file_manager_private::MountableGuest>
CreateMountableGuestList(Profile* profile);

// Converts file manager private FileCategory enum to RecentSource::FileType
// enum. Returns true if the conversion was successful, and false otherwise.
bool ToRecentSourceFileType(
    extensions::api::file_manager_private::FileCategory input_category,
    ash::RecentSource::FileType* output_type);

// Converts the given |progress| struct containing the progress of Drive's bulk
// pinning to its file manager private equivalent.
extensions::api::file_manager_private::BulkPinProgress BulkPinProgressToJs(
    const drivefs::pinning::Progress& progress);

// Converts the given GURL into an EntryData struct that can be returned by
// fileManagerPrivate.
void GURLToEntryData(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const GURL& url,
    base::OnceCallback<void(
        base::FileErrorOr<extensions::api::file_manager_private::EntryData>)>
        callback);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_UTIL_H_
