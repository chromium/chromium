// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides task related API functions.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_

#include <string>
#include <vector>

#include "chrome/browser/ash/extensions/file_manager/logged_extension_function.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "components/drive/file_errors.h"
#include "third_party/ced/src/util/encodings/encodings.h"
#include "third_party/cros_system_api/dbus/cros-disks/dbus-constants.h"

namespace extensions {

// Implements chrome.fileManagerPrivate.addMount method.
// Mounts removable devices and archive files.
class FileManagerPrivateAddMountFunction : public LoggedExtensionFunction {
 public:
  FileManagerPrivateAddMountFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.addMount",
                             FILEMANAGERPRIVATE_ADDMOUNT)

 private:
  ~FileManagerPrivateAddMountFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // Called when the encoding of a ZIP archive has been determined.
  void OnEncodingDetected(Encoding encoding);

  // Finishes mounting after encoding is detected.
  void FinishMounting();

  // Path of the device or archive to mount.
  base::FilePath path_;

  // Lowercase extension of the path to mount.
  std::string extension_;

  // Mount options.
  std::vector<std::string> options_;
};

// Implements chrome.fileManagerPrivate.cancelMounting method.
// Cancels mounting archive files.
class FileManagerPrivateCancelMountingFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateCancelMountingFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.cancelMounting",
                             FILEMANAGERPRIVATE_CANCELMOUNTING)

 private:
  ~FileManagerPrivateCancelMountingFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  void OnCancelled(ash::MountError error);
};

// Implements chrome.fileManagerPrivate.removeMount method.
// Unmounts selected volume. Expects volume id as an argument.
class FileManagerPrivateRemoveMountFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.removeMount",
                             FILEMANAGERPRIVATE_REMOVEMOUNT)

 protected:
  ~FileManagerPrivateRemoveMountFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  void OnDiskUnmounted(ash::MountError error);

  void OnSshFsUnmounted(bool ok);
};

// Implements chrome.fileManagerPrivate.getVolumeMetadataList method.
class FileManagerPrivateGetVolumeMetadataListFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getVolumeMetadataList",
                             FILEMANAGERPRIVATE_GETVOLUMEMETADATALIST)

 protected:
  ~FileManagerPrivateGetVolumeMetadataListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_
