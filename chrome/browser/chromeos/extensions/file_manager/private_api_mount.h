// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides task related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "components/drive/file_errors.h"

namespace extensions {

// Implements chrome.fileManagerPrivate.addMount method.
// Mounts removable devices and archive files.
class FileManagerPrivateAddMountFunction : public LoggedExtensionFunction {
 public:
  FileManagerPrivateAddMountFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.addMount",
                             FILEMANAGERPRIVATE_ADDMOUNT)

 protected:
  ~FileManagerPrivateAddMountFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // Part of Run(). Called after EnsureReadableFilePermissionAsync or when the
  // file is on an external drive.
  void RunAfterEnsureReadableFilePermission(const base::FilePath& display_name,
                                            drive::FileError error,
                                            const base::FilePath& file_path);

  const ChromeExtensionFunctionDetails chrome_details_;
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

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_
