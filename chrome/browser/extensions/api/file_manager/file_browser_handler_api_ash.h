// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File contains the fileBrowserHandlerInternal.selectFile extension function.
// The function prompts user to select a file path to be used by the caller. It
// will fail if it isn't invoked by a user gesture (e.g. a mouse click or a
// keyboard key press).
// Note that the target file is never actually created by this function, even
// if the selected path doesn't exist.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_ASH_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/file_manager/file_browser_handler_api.h"
#include "extensions/browser/extension_function.h"

namespace file_manager {

class FileSelector;
class FileSelectorFactory;

namespace util {
struct EntryDefinition;
}

}  // namespace file_manager

// Note that this class is not in 'file_manager' namespace to be consistent with
// all other extension functions registered in
// chrome/common/extensions/api/generated_api.cc being in the global namespace.
//
// The fileBrowserHandlerInternal.selectFile extension function implementation
// for ash. See the file description for more info.
class FileBrowserHandlerInternalSelectFileFunctionAsh
    : public FileBrowserHandlerInternalSelectFileFunction {
 public:
  // Default constructor used in production code that instantiates production
  // FileSelectorFactory and assigns |user_gesture_check_enabled| to true.
  FileBrowserHandlerInternalSelectFileFunctionAsh();

  // Test-only constructor to inject test FileSelectorFactory (owned by the
  // instance). Passed |user_gesture_check_enabled| can be false, so that
  // extension functions being tested can run even if they're not invoked by
  // user gesture.
  FileBrowserHandlerInternalSelectFileFunctionAsh(
      file_manager::FileSelectorFactory* file_selector_factory,
      bool enable_user_gesture_check);

  // Handler to receive results from FileSelector. On successfully choosing
  // a file path, grants file access permissions for the selected file, and
  // responds to the original API call.
  // |success| Whether the file path was successfully selected.
  // |full_path| The selected file path if successful. Ignored if unsuccessful.
  void OnFilePathSelected(bool success, const base::FilePath& full_path);

 protected:
  // The class is ref counted, so destructor should not be public.
  ~FileBrowserHandlerInternalSelectFileFunctionAsh() override;

  // ExtensionFunction implementation.
  // Runs the extension function implementation.
  ResponseAction Run() override;

 private:
  // Respond to the API with selected entry definition.
  void RespondEntryDefinition(
      const file_manager::util::EntryDefinition& entry_definition);

  // Creates dictionary value that will be used to as the extension function's
  // callback argument and ends extension function execution by calling
  // |Respond()|.
  // The |results_| value will be set to dictionary containing two properties:
  // * boolean 'success', which will be equal to |success|.
  // * object 'entry', which will be set only when |success| is true, and the
  //   conversion to |entry_definition| was successful. In such case, it will
  //   contain information needed to create a FileEntry object for the selected
  //   file.
  void RespondWith(const file_manager::util::EntryDefinition& entry_definition,
                   bool success);

  // Factory used to create FileSelector to be used for prompting user to select
  // file.
  std::unique_ptr<file_manager::FileSelectorFactory> file_selector_factory_;

  // Whether user gesture check is enabled. This should be false only in tests.
  bool user_gesture_check_enabled_;

  // List of permissions and paths that have to be granted for the selected
  // files.
  std::vector<std::pair<base::FilePath, int>> permissions_to_grant_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_ASH_H_
