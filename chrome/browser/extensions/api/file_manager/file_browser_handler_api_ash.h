// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_ASH_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/api/file_manager/file_browser_handler_api.h"

namespace file_manager {

class FileSelectorFactory;

namespace util {
struct EntryDefinition;
}  // namespace util

}  // namespace file_manager

// The fileBrowserHandlerInternal.selectFile extension function implementation
// for Ash. See the file description for more info.
// This class is not in file_manager namespace to be consistent with other
// extension functions, which are in the global namespace, and registered in
// chrome/common/extensions/api/generated_api.cc.
class FileBrowserHandlerInternalSelectFileFunctionAsh
    : public FileBrowserHandlerInternalSelectFileFunction {
 public:
  FileBrowserHandlerInternalSelectFileFunctionAsh();

  FileBrowserHandlerInternalSelectFileFunctionAsh(
      file_manager::FileSelectorFactory* file_selector_factory,
      bool enable_user_gesture_check);

  // FileBrowserHandlerInternalSelectFileFunction overrides.
  void OnFilePathSelected(bool success,
                          const base::FilePath& full_path) override;

 protected:
  // The class is ref counted, so destructor should not be public.
  ~FileBrowserHandlerInternalSelectFileFunctionAsh() override;

 private:
  // Respond to the API with selected entry definition.
  void RespondEntryDefinition(
      const file_manager::util::EntryDefinition& entry_definition);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_BROWSER_HANDLER_API_ASH_H_
