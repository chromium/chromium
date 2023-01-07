// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_LACROS_H_

#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/file_manager/file_browser_handler_api.h"
#include "extensions/browser/extension_function.h"

// The fileBrowserHandlerInternal.selectFile extension function implementation
// for Lacros.
class FileBrowserHandlerInternalSelectFileFunctionLacros
    : public FileBrowserHandlerInternalSelectFileFunction {
 public:
  FileBrowserHandlerInternalSelectFileFunctionLacros();

  // FileBrowserHandlerInternalSelectFileFunction overrides.
  void OnFilePathSelected(bool success,
                          const base::FilePath& full_path) override;

 protected:
  // The class is ref counted, so destructor should not be public.
  ~FileBrowserHandlerInternalSelectFileFunctionLacros() override;

 private:
  DECLARE_EXTENSION_FUNCTION("fileBrowserHandlerInternal.selectFile",
                             FILEBROWSERHANDLERINTERNAL_SELECTFILE)
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_LACROS_H_
