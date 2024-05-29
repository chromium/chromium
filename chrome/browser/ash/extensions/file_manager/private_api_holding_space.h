// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_HOLDING_SPACE_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_HOLDING_SPACE_H_

#include "chromeos/ash/components/file_manager/indexing/search_results.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class FileManagerPrivateInternalToggleAddedToHoldingSpaceFunction
    : public ExtensionFunction {
 public:
  FileManagerPrivateInternalToggleAddedToHoldingSpaceFunction();

  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.toggleAddedToHoldingSpace",
      FILEMANAGERPRIVATEINTERNAL_TOGGLEADDEDTOHOLDINGSPACE)

 protected:
  ~FileManagerPrivateInternalToggleAddedToHoldingSpaceFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class FileManagerPrivateGetHoldingSpaceStateFunction
    : public ExtensionFunction {
 public:
  FileManagerPrivateGetHoldingSpaceStateFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getHoldingSpaceState",
                             FILEMANAGERPRIVATEINTERNAL_GETHOLDINGSPACESTATE)

 protected:
  ~FileManagerPrivateGetHoldingSpaceStateFunction() override;
  void OnSearchResult(::ash::file_manager::SearchResults result);

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_HOLDING_SPACE_H_
