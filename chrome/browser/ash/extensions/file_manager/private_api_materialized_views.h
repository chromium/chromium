// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MATERIALIZED_VIEWS_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MATERIALIZED_VIEWS_H_

#include "base/files/file_error_or.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class FileManagerPrivateGetMaterializedViewsFunction
    : public ExtensionFunction {
 public:
  FileManagerPrivateGetMaterializedViewsFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getMaterializedViews",
                             FILEMANAGERPRIVATE_GETMATERIALIZEDVIEWS)

 protected:
  ~FileManagerPrivateGetMaterializedViewsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class FileManagerPrivateReadMaterializedViewFunction
    : public ExtensionFunction {
 public:
  FileManagerPrivateReadMaterializedViewFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.readMaterializedView",
                             FILEMANAGERPRIVATE_READMATERIALIZEDVIEW)

 protected:
  ~FileManagerPrivateReadMaterializedViewFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnEntryDataRetrieved(
      std::vector<base::FileErrorOr<api::file_manager_private::EntryData>>
          entry_results);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MATERIALIZED_VIEWS_H_
