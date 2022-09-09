// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_SELECTOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_SELECTOR_H_

#include <string>
#include <vector>

#include "base/callback.h"

class Browser;

namespace base {
class FilePath;
}  // namespace base

namespace file_manager {

// A class to manage UI flow of a "Save as" file selection dialog, intended to
// be used in a "fire-and-forget" fashion. As such, derived classes are expected
// to be self-deleting.
class FileSelector {
 public:
  using OnSelectedCallback =
      base::OnceCallback<void(bool /* success */,
                              const base::FilePath& /* full_path */)>;

  FileSelector() = default;
  FileSelector(const FileSelector&) = delete;
  FileSelector& operator=(const FileSelector&) = delete;
  virtual ~FileSelector() = default;

  // Starts the "Save as" file selection flow in a window bound to |browser|
  // (expected to outlive this instance). Results are asynchronously passed to
  // |callback| on completion. By default, |suggested_name| is shown in the UI.
  // |allowed_extensions| specifies an allowlist of *file* extensions visible
  // and selectable by the UI. These extensions should not include '.' (as
  // required by ui::SelectFileDialog()).
  virtual void SelectFile(const base::FilePath& suggested_name,
                          const std::vector<std::string>& allowed_extensions,
                          Browser* browser,
                          OnSelectedCallback callback) = 0;
};

// Factory for FileSelector to enable mocking in tests.
class FileSelectorFactory {
 public:
  virtual ~FileSelectorFactory() = default;

  // Creates a FileSelector instance.
  virtual FileSelector* CreateFileSelector() const = 0;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_SELECTOR_H_
