// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File contains the fileBrowserHandlerInternal.selectFile extension function.
// The function prompts user to select a file path to be used by the caller. It
// will fail if it isn't invoked by a user gesture (e.g. a mouse click or a
// keyboard key press).
// Note that the target file is never actually created by this function, even
// if the selected path doesn't exist.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_HANDLER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_HANDLER_API_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "extensions/browser/extension_function.h"

class Browser;
class FileBrowserHandlerInternalSelectFileFunction;

namespace file_manager {

namespace util {
struct EntryDefinition;
}

// Interface that is used by FileBrowserHandlerInternalSelectFileFunction to
// select the file path that should be reported back to the extension function
// caller.  Nobody will take the ownership of the interface implementation, so
// it should delete itself once it's done.
class FileSelector {
 public:
  virtual ~FileSelector() = default;

  // Starts the file selection. It should prompt user to select a file path.
  // Once the selection is made it should asynchronously call
  // |function_->OnFilePathSelected| with the selection information.
  // User should be initially suggested to select file named |suggested_name|.
  // |allowed_extensions| specifies the file extensions allowed to be shown,
  // and selected. Extensions should not include '.'. This spec comes from
  // ui::SelectFileDialog() which takes extensions without '.'.
  //
  // Selection UI should be displayed using |browser|. |browser| should outlive
  // the interface implementation.
  // |function| if the extension function that called the method and needs to
  // be notified of user action. The interface implementation should keep a
  // reference to the function until it is notified (extension function
  // implementations are ref counted).
  // |SelectFile| will be called at most once by a single extension function.
  // The interface implementation should delete itself after the extension
  // function is notified of file selection result.
  virtual void SelectFile(
      const base::FilePath& suggested_name,
      const std::vector<std::string>& allowed_extensions,
      Browser* browser,
      FileBrowserHandlerInternalSelectFileFunction* function) = 0;
};

// Interface that is used by FileBrowserHandlerInternalSelectFileFunction to
// create a FileSelector it can use to select a file path.
class FileSelectorFactory {
 public:
  virtual ~FileSelectorFactory() = default;

  // Creates a FileSelector instance for the
  // FileBrowserHandlerInternalSelectFileFunction.
  virtual FileSelector* CreateFileSelector() const = 0;
};

}  // namespace file_manager


// Note that this class is not in 'file_manager' class to be consistent with
// all other extension functions registered in
// chrome/common/extensions/api/generated_api.cc being in the global namespace.
//
// The fileBrowserHandlerInternal.selectFile extension function implementation.
// See the file description for more info.
class FileBrowserHandlerInternalSelectFileFunction
    : public extensions::LoggedExtensionFunction {
 public:
  // Default constructor used in production code.
  // It will create its own FileSelectorFactory implementation, and set the
  // value of |user_gesture_check_enabled| to true.
  FileBrowserHandlerInternalSelectFileFunction();

  // This constructor should be used only in tests to inject test file selector
  // factory and to allow extension function to run even if it hasn't been
  // invoked by user gesture.
  // Created object will take the ownership of the |file_selector_factory|.
  FileBrowserHandlerInternalSelectFileFunction(
      file_manager::FileSelectorFactory* file_selector_factory,
      bool enable_user_gesture_check);

  // Called by FileSelector implementation when the user selects the file's
  // file path. File access permissions for the selected file are granted and
  // caller is notified of the selection result after this method is called.
  // |success| Whether the path was selected.
  // |full_path| The selected file path if one was selected. It is ignored if
  // the selection did not succeed.
  void OnFilePathSelected(bool success, const base::FilePath& full_path);

 protected:
  // The class is ref counted, so destructor should not be public.
  ~FileBrowserHandlerInternalSelectFileFunction() override;

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
  // Whether user gesture check is disabled. This should be true only in tests.
  bool user_gesture_check_enabled_;

  // List of permissions and paths that have to be granted for the selected
  // files.
  std::vector<std::pair<base::FilePath, int> > permissions_to_grant_;

  DECLARE_EXTENSION_FUNCTION("fileBrowserHandlerInternal.selectFile",
                             FILEBROWSERHANDLERINTERNAL_SELECTFILE)
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_HANDLER_API_H_
