// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_file_watcher.h"
#include "chrome/browser/ash/guest_os/public/types.h"

class Profile;

namespace guest_os {

class GuestOsMountProviderInner;
class GuestOsMountProvider {
 public:
  using PrepareCallback = base::OnceCallback<
      void(bool success, int cid, int port, base::FilePath homedir)>;
  GuestOsMountProvider();
  virtual ~GuestOsMountProvider();

  GuestOsMountProvider(const GuestOsMountProvider&) = delete;
  GuestOsMountProvider& operator=(const GuestOsMountProvider&) = delete;

  // Get the profile for this provider.
  virtual Profile* profile() = 0;

  // The localised name to show in UI elements such as the files app sidebar.
  virtual std::string DisplayName() = 0;

  virtual guest_os::GuestId GuestId() = 0;

  // The type of VM which this provider creates mounts for. Needed for e.g.
  // enterprise policy which applies different rules to each disk volume
  // depending on the underlying VM.
  virtual VmType vm_type() = 0;

  // Requests the provider to mount its volume.
  void Mount(base::OnceCallback<void(bool)> callback);

  // Requests the provider to unmount.
  void Unmount();

  // Creates a file watcher for the given path, specified by `mount_path` as the
  // path to where the volume is mounted and `relative_path` is the path to
  // watch relative to `mount_path`. The watcher starts off idle, call Watch to
  // start watching.
  virtual std::unique_ptr<GuestOsFileWatcher> CreateFileWatcher(
      base::FilePath mount_path,
      base::FilePath relative_path) = 0;

 protected:
  // Called prior to mounting a volume, for the mount provider to do any
  // needed setup (e.g. ensure the VM is running). Must call `callback` once
  // finished to actually mount. On error set `success` to false and the
  // mount will be aborted, on success set `success` to true, set `cid` and
  // `port` to the address that the guests's sftp server is listening on,
  // and set `homedir` to the path, relative to the VM root, which is being
  // shared.
  virtual void Prepare(PrepareCallback callback) = 0;

 private:
  std::unique_ptr<GuestOsMountProviderInner> callback_;
  base::WeakPtrFactory<GuestOsMountProvider> weak_ptr_factory_{this};
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_H_
