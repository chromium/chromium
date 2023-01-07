// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_FILE_WATCHER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_FILE_WATCHER_H_

#include "base/files/file_path_watcher.h"
#include "chrome/browser/ash/file_manager/file_watcher.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"

namespace guest_os {

class GuestOsFileWatcher : public ash::CiceroneClient::Observer {
 public:
  // Creates a watcher for the given `guest_id`, which uses garcon and is
  // sharing a directory mounted under `mount_path`, that watches for changes to
  // `path` where `path` is relative to the share root. The created watcher is
  // in an idle state, call Watch to actually start watching.
  GuestOsFileWatcher(std::string owner_id,
                     GuestId guest_id,
                     base::FilePath mount_path,
                     base::FilePath path);

  ~GuestOsFileWatcher() override;

  // Call this to start watching the path, destroy this watcher to stop
  // watching. Will run `callback` once the watch is set up with either true on
  // success or false on failure. If the watch is successfully set up,
  // `file_watcher_callback` will be invoked on file changes with the path
  // *outside* that's changed. For example, if the volume is mounted at
  // /mnt/foo, the folder the VM is sharing is rooted at /home/user, and the
  // watch is for baz, then if /home/user/baz is changed `file_watcher_callback`
  // will be called with /mnt/foo/baz.
  void Watch(base::FilePathWatcher::Callback file_watcher_callback,
             file_manager::FileWatcher::BoolCallback callback);

  // ash::CiceroneClient::Observer override.
  void OnFileWatchTriggered(
      const vm_tools::cicerone::FileWatchTriggeredSignal& signal) override;

 private:
  std::string owner_id_;
  const guest_os::GuestId guest_id_;

  // Runs this callback when the
  base::FilePathWatcher::Callback file_watcher_callback_;

  // The location on the host filesystem where the mount is rooted.
  const base::FilePath mount_path_;

  // The path, relative to the mount rooot, which is to be watched.
  const base::FilePath path_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_FILE_WATCHER_H_
