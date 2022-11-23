// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_mount_provider.h"

#include "base/logging.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/profiles/profile_helper.h"

namespace bruschetta {

BruschettaMountProvider::BruschettaMountProvider(Profile* profile,
                                                 guest_os::GuestId guest_id)
    : profile_(profile), guest_id_(guest_id) {}
BruschettaMountProvider::~BruschettaMountProvider() = default;

// guest_os::GuestOsMountProvider overrides.
Profile* BruschettaMountProvider::profile() {
  return profile_;
}

std::string BruschettaMountProvider::DisplayName() {
  return kBruschettaDisplayName;
}

guest_os::GuestId BruschettaMountProvider::GuestId() {
  return guest_id_;
}

guest_os::VmType BruschettaMountProvider::vm_type() {
  return guest_os::VmType::BRUSCHETTA;
}

std::unique_ptr<guest_os::GuestOsFileWatcher>
BruschettaMountProvider::CreateFileWatcher(base::FilePath mount_path,
                                           base::FilePath relative_path) {
  return std::make_unique<guest_os::GuestOsFileWatcher>(
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_), guest_id_,
      std::move(mount_path), std::move(relative_path));
}

// guest_os::GuestOsMountProvider override.
void BruschettaMountProvider::Prepare(PrepareCallback callback) {
  auto* service = BruschettaService::GetForProfile(profile_);
  auto launcher = service->GetLauncher(guest_id_.vm_name);
  if (launcher) {
    launcher->EnsureRunning(base::BindOnce(&BruschettaMountProvider::OnRunning,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           std::move(callback)));
  } else {
    std::move(callback).Run(false, {}, {}, {});
  }
}

void BruschettaMountProvider::OnRunning(PrepareCallback callback,
                                        BruschettaResult result) {
  if (result != BruschettaResult::kSuccess) {
    LOG(ERROR) << "Error launching Bruschetta: " << static_cast<int>(result);
    std::move(callback).Run(false, 0, 0, base::FilePath());
    return;
  }
  auto* tracker = guest_os::GuestOsSessionTracker::GetForProfile(profile_);

  auto info = tracker->GetInfo(guest_id_);
  if (!info) {
    // Shouldn't happen unless you managed to shutdown the VM at the same
    // instant as you booted it.
    LOG(WARNING) << "Unrecognised/not running guest, unable to mount";
    std::move(callback).Run(false, 0, 0, base::FilePath());
    return;
  }
  unmount_subscription_ = tracker->RunOnShutdown(
      guest_id_, base::BindOnce(&BruschettaMountProvider::Unmount,
                                weak_ptr_factory_.GetWeakPtr()));
  std::move(callback).Run(true, info->cid, info->sftp_vsock_port,
                          info->homedir);
}

}  // namespace bruschetta
