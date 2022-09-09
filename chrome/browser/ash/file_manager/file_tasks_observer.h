// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_OBSERVER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_OBSERVER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/observer_list_types.h"

namespace file_manager {
namespace file_tasks {

class FileTasksObserver : public base::CheckedObserver {
 public:
  enum class OpenType {
    // Launch from the files app or the Downloads page.
    kLaunch,

    // Chosen from a file-open dialog.
    kOpen,

    // Chosen from a file-save-as dialog.
    kSaveAs,

    // A file was downloaded in Chrome.
    kDownload,
  };

  struct FileOpenEvent {
    base::FilePath path;
    OpenType open_type;
  };

  virtual void OnFilesOpened(const std::vector<FileOpenEvent>& file_opens) {}
};

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_OBSERVER_H_
