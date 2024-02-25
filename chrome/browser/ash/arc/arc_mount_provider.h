// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ARC_MOUNT_PROVIDER_H_
#define CHROME_BROWSER_ASH_ARC_ARC_MOUNT_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"

namespace guest_os {
class GuestOsFileWatcher;
}

namespace arc {

// This class is responsible for mounting the sshfs version of Play files mount.
class ArcMountProvider : public guest_os::GuestOsMountProvider {
 public:
  ArcMountProvider(Profile* profile, int cid);

  ArcMountProvider(const ArcMountProvider&) = delete;
  ArcMountProvider& operator=(const ArcMountProvider&) = delete;

  ~ArcMountProvider() override;

  // GuestOsMountProvider overrides
  Profile* profile() override;
  std::string DisplayName() override;
  guest_os::GuestId GuestId() override;
  guest_os::VmType vm_type() override;

  std::unique_ptr<guest_os::GuestOsFileWatcher> CreateFileWatcher(
      base::FilePath mount_path,
      base::FilePath relative_path) override;

 protected:
  // GuestOsMountProvider override. Make sure Arc is running, then get
  // address info e.g. cid and vsock port.
  void Prepare(PrepareCallback callback) override;

 private:
  const raw_ptr<Profile> profile_;
  const int cid_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcMountProvider> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ARC_MOUNT_PROVIDER_H_
