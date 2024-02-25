// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MOUNT_PROVIDER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MOUNT_PROVIDER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"

namespace guest_os {
class GuestOsFileWatcher;
}  // namespace guest_os

namespace crostini {

class CrostiniMountProvider : public guest_os::GuestOsMountProvider,
                              public ContainerShutdownObserver {
 public:
  explicit CrostiniMountProvider(Profile* profile,
                                 guest_os::GuestId container_id);

  CrostiniMountProvider(const CrostiniMountProvider&) = delete;
  CrostiniMountProvider& operator=(const CrostiniMountProvider&) = delete;

  ~CrostiniMountProvider() override;

  // GuestOsMountProvider overrides
  Profile* profile() override;
  std::string DisplayName() override;
  guest_os::GuestId GuestId() override;
  guest_os::VmType vm_type() override;

  std::unique_ptr<guest_os::GuestOsFileWatcher> CreateFileWatcher(
      base::FilePath mount_path,
      base::FilePath relative_path) override;

  // ContainerShutdownObserver override
  void OnContainerShutdown(const guest_os::GuestId& container_id) override;

 protected:
  // GuestOsMountProvider override. Make sure Crostini's running, then get
  // address info e.g. cid and vsock port.
  void Prepare(PrepareCallback callback) override;

 private:
  void OnRestarted(PrepareCallback callback, CrostiniResult result);

  raw_ptr<Profile> profile_;
  guest_os::GuestId container_id_;
  base::ScopedObservation<CrostiniManager, ContainerShutdownObserver>
      container_shutdown_observer_{this};
  base::CallbackListSubscription subscription_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrostiniMountProvider> weak_ptr_factory_{this};
};
}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MOUNT_PROVIDER_H_
