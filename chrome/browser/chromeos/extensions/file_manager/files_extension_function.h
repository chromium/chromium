// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains base classes for fileManagerPrivate API.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILES_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILES_EXTENSION_FUNCTION_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace extensions {

// The base class for all extensions function used by Files App. The main
// reason is to provide Files SWA with the ability to execute
// chrome.fileManagerPrivate APIs that must have access to an extension ID.
class FilesExtensionFunction : public ExtensionFunction {
 public:
  FilesExtensionFunction();

 protected:
  ~FilesExtensionFunction() override;

  /**
   * Always returns the ID of the Files.app, a Chrome App.
   */
  const std::string& file_app_id() const { return file_app_id_; }

  /**
   * Returns whether or not this extension function is run by File SWA.
   */
  bool is_swa_mode() const;

  /**
   * Returns the ID of the calling extension or the Files.app, if the
   * extension ID is not available.
   */
  const std::string& extension_id_or_file_app_id() const;

 private:
  // The ID of the legacy Files.app.
  const std::string file_app_id_;
  // The URL of the Files system web application.
  const std::string swa_url_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILES_EXTENSION_FUNCTION_H_
