// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_WATCHER_H_
#define CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_WATCHER_H_

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "chrome/common/child_module/child_module_helper.h"

namespace child_module {

// Helper class that runs entirely on a background sequenced task runner
// (typically via base::SequenceBound). It monitors the modules directory for
// staged child modules and notifies a callback whenever the set of available
// versions changes.
class ChildModuleWatcher {
 public:
  using VersionSetCallback = base::RepeatingCallback<void(VersionSet)>;

  ChildModuleWatcher(base::FilePath modules_dir,
                     VersionSetCallback on_version_set_changed);
  ChildModuleWatcher(const ChildModuleWatcher&) = delete;
  ChildModuleWatcher& operator=(const ChildModuleWatcher&) = delete;
  ~ChildModuleWatcher();

 private:
  void OnDirectoryChanged(const base::FilePath& path, bool error);
  void Scan();

  const base::FilePath modules_dir_;
  VersionSetCallback on_version_set_changed_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::FilePathWatcher file_path_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::DelayTimer debounce_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingTimer periodic_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  VersionSet current_versions_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ChildModuleWatcher> weak_factory_{this};
};

}  // namespace child_module

#endif  // CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_WATCHER_H_
