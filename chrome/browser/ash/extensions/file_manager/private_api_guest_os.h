// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides Guest OS API functions

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_GUEST_OS_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_GUEST_OS_H_

#include "chrome/browser/ash/extensions/file_manager/logged_extension_function.h"
#include "chrome/common/extensions/api/file_manager_private.h"

namespace extensions {

// Implements the chrome.fileManagerPrivate.listMountableGuests method.
class FileManagerPrivateListMountableGuestsFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.listMountableGuests",
                             FILEMANAGERPRIVATE_LISTMOUNTABLEGUESTS)

 protected:
  ~FileManagerPrivateListMountableGuestsFunction() override = default;

 private:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.mountGuest method.
// Mounts a Guest OS.
class FileManagerPrivateMountGuestFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.mountGuest",
                             FILEMANAGERPRIVATE_MOUNTGUEST)
  FileManagerPrivateMountGuestFunction();

  FileManagerPrivateMountGuestFunction(
      const FileManagerPrivateMountGuestFunction&) = delete;
  FileManagerPrivateMountGuestFunction& operator=(
      const FileManagerPrivateMountGuestFunction&) = delete;

 private:
  ~FileManagerPrivateMountGuestFunction() override;

  // Callback for when the mount event completes. `success` is true if the mount
  // succeeded, false on error.
  void MountCallback(bool success);

  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_GUEST_OS_H_
