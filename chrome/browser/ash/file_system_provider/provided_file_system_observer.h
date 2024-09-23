// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_OBSERVER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_OBSERVER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/file_system_provider/cloud_file_info.h"
#include "chrome/browser/ash/file_system_provider/watcher.h"
#include "storage/browser/file_system/watcher_manager.h"

namespace ash::file_system_provider {

class ProvidedFileSystemInfo;

// Observer class to be notified about changes happened to the provided file
// system, including watched entries.
class ProvidedFileSystemObserver {
 public:
  struct Change;

  // List of changes.
  typedef std::vector<Change> Changes;

  // Describes a change related to a watched entry.
  struct Change {
    Change(base::FilePath entry_path,
           storage::WatcherManager::ChangeType change_type,
           std::unique_ptr<CloudFileInfo> cloud_file_info);

    // Not copyable.
    Change(const Change&) = delete;
    Change& operator=(const Change&) = delete;

    // Movable
    Change(Change&&);

    ~Change();

    base::FilePath entry_path;
    storage::WatcherManager::ChangeType change_type;
    std::unique_ptr<CloudFileInfo> cloud_file_info;
  };

  // Called when a watched entry is changed, including removals. |callback|
  // *must* be called after the entry change is handled. Once all observers
  // call the callback, the tag will be updated and OnWatcherTagUpdated
  // called. The reference to |changes| is valid at least as long as |callback|.
  virtual void OnWatcherChanged(const ProvidedFileSystemInfo& file_system_info,
                                const Watcher& watcher,
                                storage::WatcherManager::ChangeType change_type,
                                const Changes& changes,
                                base::OnceClosure callback) = 0;

  // Called after the tag value is updated for the watcher.
  virtual void OnWatcherTagUpdated(
      const ProvidedFileSystemInfo& file_system_info,
      const Watcher& watcher) = 0;

  // Called after the list of watchers is changed.
  virtual void OnWatcherListChanged(
      const ProvidedFileSystemInfo& file_system_info,
      const Watchers& watchers) = 0;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_OBSERVER_H_
