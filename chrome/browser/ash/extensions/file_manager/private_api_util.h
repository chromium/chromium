// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utility functions for fileManagerPrivate API.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_UTIL_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_UTIL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
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
}

namespace content {
class RenderFrameHost;
}

namespace drive {
class EventLogger;
}

namespace extensions {
namespace api {
namespace file_manager_private {
struct EntryProperties;
struct IconSet;
struct VolumeMetadata;
struct MountableGuest;
}
}
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
  // To request specific properties, pass the requested_properties set.
  // Note: Passing an empty set retrieves all available properties.
  static void Start(
      const storage::FileSystemURL& file_system_url,
      Profile* const profile,
      const std::set<extensions::api::file_manager_private::EntryPropertyName>
          requested_properties,
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
      const std::set<extensions::api::file_manager_private::EntryPropertyName>
          requested_properties,
      ResultCallback callback);
  void StartProcess();
  void OnGetFileInfo(drive::FileError error,
                     drivefs::mojom::FileMetadataPtr metadata);
  void CompleteGetEntryProperties(drive::FileError error);

  // Given parameters.
  ResultCallback callback_;
  const storage::FileSystemURL file_system_url_;
  Profile* const running_profile_;
  // Note: when empty, all properties are returned.
  const std::set<extensions::api::file_manager_private::EntryPropertyName>
      requested_properties_;
  // If only some of these properties are being requested, we don't need to get
  // metadata from DriveFS as they are already cached in the SyncStatusTracker.
  const std::set<extensions::api::file_manager_private::EntryPropertyName>
      locally_available_properties_ = {
          extensions::api::file_manager_private::ENTRY_PROPERTY_NAME_SYNCSTATUS,
          extensions::api::file_manager_private::ENTRY_PROPERTY_NAME_PROGRESS};

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
// type FileSystemBackend handles, returns an empty
// FilePath. |render_frame_host| and |profile| are needed to obtain the
// FileSystemContext currently in use.
//
// Local paths will look like "/home/chronos/user/Downloads/foo/bar.txt" or
// "/special/drive/foo/bar.txt".
base::FilePath GetLocalPathFromURL(content::RenderFrameHost* render_frame_host,
                                   Profile* profile,
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

// Gets the information for |file_urls|.
void GetSelectedFileInfo(content::RenderFrameHost* render_frame_host,
                         Profile* profile,
                         const std::vector<GURL>& file_urls,
                         GetSelectedFileInfoLocalPathOption local_path_option,
                         GetSelectedFileInfoCallback callback);

// Get event logger to chrome://drive-internals page for the |profile|.
drive::EventLogger* GetLogger(Profile* profile);

std::vector<extensions::api::file_manager_private::MountableGuest>
CreateMountableGuestList(Profile* profile);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_UTIL_H_
