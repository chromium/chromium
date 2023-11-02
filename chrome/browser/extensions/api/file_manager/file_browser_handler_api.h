// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_browser_handler_internal.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"

namespace file_manager {
class FileSelectorFactory;
}  // namespace file_manager

// Handler for the fileBrowserHandlerInternal.selectFile() extension function,
// which prompts user to select a file path to be used by the caller. This is an
// abstract base class implementing common code used by ash and lacros.
// selectFile() must be invoked by a user gesture (e.g. a mouse click or a key
// press). Note: If the selected file does not exist, the API function does NOT
// create it.
class FileBrowserHandlerInternalSelectFileFunction : public ExtensionFunction {
 public:
  // Default constructor that instantiates production FileSelectorFactory and
  // assigns |user_gesture_check_enabled| to true.
  FileBrowserHandlerInternalSelectFileFunction();

  // Test-only constructor to inject test FileSelectorFactory (owned by the
  // instance). Passed |user_gesture_check_enabled| can be false to allow
  // extension functions under test to run even if they're not invoked by user
  // gesture.
  FileBrowserHandlerInternalSelectFileFunction(
      file_manager::FileSelectorFactory* file_selector_factory,
      bool enable_user_gesture_check);

  // Handler to receive results from FileSelector. On successfully choosing a
  // file path, grants file access permissions for the selected file, and
  // responds to the original API call. |success| specifies whether the file
  // path was successfully selected. |full_path| specifies the selected file
  // path if successful, and should be ignored if unsuccessful.
  virtual void OnFilePathSelected(bool success,
                                  const base::FilePath& full_path) = 0;

 protected:
  // The class is ref counted, so the destructor should not be public.
  ~FileBrowserHandlerInternalSelectFileFunction() override;

  // ExtensionFunction implementation.
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("fileBrowserHandlerInternal.selectFile",
                             FILEBROWSERHANDLERINTERNAL_SELECTFILE)

  // Finishes execution, passing results (see RespondWithResult()) indicating
  // failure.
  void RespondWithFailure();

  // Finishes execution, forwarding result that get translated to a dictionary
  // value with the following fields:
  // * boolean |success|, indicating whether user has chosen a file, and
  //   subsequent operations (e.g., granting permission) are successful.
  // * object |entry|, set only when |success| is true, containing data needed
  //   by Ash to create a FileEntry object for the selected file.
  // * object |entry_for_get_file|, set only when |success| is true, containing
  //   data needed by Lacros to create a FileEntry object for the selected file.
  void RespondWithResult(const extensions::api::file_browser_handler_internal::
                             SelectFile::Results::Result& result);

  // Factory to create FileSelector for prompting user to select file.
  std::unique_ptr<file_manager::FileSelectorFactory> file_selector_factory_;

  // Whether user gesture check is enabled. This should be false only in tests.
  bool user_gesture_check_enabled_;
};

// generated_api_registration.cc refers to the base class. Derived classes
// (ash and lacros) are mutually-exclusively compiled, with separate version of
// the following to return the proper platform-dependent instance.
template <>
scoped_refptr<ExtensionFunction>
NewExtensionFunction<FileBrowserHandlerInternalSelectFileFunction>();

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_H_
