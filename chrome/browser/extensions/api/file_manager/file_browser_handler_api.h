// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"

// This is an abstract class which ash and lacros both inherit from to provide
// their own implementation of the fileBrowserHandlerInternal.selectFile
// extension function.
class FileBrowserHandlerInternalSelectFileFunction : public ExtensionFunction {
 public:
  FileBrowserHandlerInternalSelectFileFunction() = default;

 protected:
  // The class is ref counted, so the destructor should not be public.
  ~FileBrowserHandlerInternalSelectFileFunction() override = default;

 private:
  DECLARE_EXTENSION_FUNCTION("fileBrowserHandlerInternal.selectFile",
                             FILEBROWSERHANDLERINTERNAL_SELECTFILE)
};

// The ash and lacros versions of this API implement this by returning their own
// subclass of FileBrowserHandlerInternalSelectFileFunction. This is required so
// that generated_api_registration.cc can get the right implementation for the
// current platform.
template <>
scoped_refptr<ExtensionFunction>
NewExtensionFunction<FileBrowserHandlerInternalSelectFileFunction>();

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_H_
