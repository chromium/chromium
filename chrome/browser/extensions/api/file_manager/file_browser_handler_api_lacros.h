// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_LACROS_H_

#include "chrome/browser/extensions/api/file_manager/file_browser_handler_api.h"
#include "extensions/browser/extension_function.h"

// The fileBrowserHandlerInternal.selectFile extension function implementation
// for Lacros.
class FileBrowserHandlerInternalSelectFileFunctionLacros
    : public FileBrowserHandlerInternalSelectFileFunction {
 public:
  FileBrowserHandlerInternalSelectFileFunctionLacros();

 protected:
  // The class is ref counted, so destructor should not be public.
  ~FileBrowserHandlerInternalSelectFileFunctionLacros() override;

  // ExtensionFunction implementation.
  // Runs the extension function implementation.
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("fileBrowserHandlerInternal.selectFile",
                             FILEBROWSERHANDLERINTERNAL_SELECTFILE)
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_LACROS_H_
