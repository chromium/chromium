// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implements chrome.fileManagerPrivateInternal.getVolumeRoot function.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FMPI_GET_VOLUME_ROOT_FUNCTION_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FMPI_GET_VOLUME_ROOT_FUNCTION_H_

#include "chrome/common/extensions/api/file_manager_private.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace file_manager {
namespace util {
struct EntryDefinition;
}  // namespace util
}  // namespace file_manager

namespace extensions {

// Implements the chrome.fileManagerPrivateInternal.getVolumeRoot method. This
// function, for a given volume ID, returns a DirectoryEntry object referencing
// directly the root directory of th external file system. As it is part of a
// highly privileged API, it does not use allowlist checks (since these are
// already granted by the virtue of this API being enabled).
class FileManagerPrivateInternalGetVolumeRootFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getVolumeRoot",
                             FILEMANAGERPRIVATEINTERNAL_GETVOLUMEROOT)

 protected:
  ~FileManagerPrivateInternalGetVolumeRootFunction() override = default;

 private:
  // ExtensionFunction overrides. Checks function parameters and attempts to
  // grant the caller access to the specified volume and constructs an object
  // representing the root directory of the volume.
  ResponseAction Run() override;

  // Callback method that returns the result back to the invoking JavaScript.
  // The |entry| parameter is either a valid entry definition, or an entry
  // with the error field set.
  void OnRequestDone(const file_manager::util::EntryDefinition& entry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FMPI_GET_VOLUME_ROOT_FUNCTION_H_
