// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_terminal_provider.h"

#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_terminal.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"

namespace crostini {

CrostiniTerminalProvider::CrostiniTerminalProvider(
    Profile* profile,
    guest_os::GuestId container_id)
    : profile_(profile), container_id_(container_id) {}
CrostiniTerminalProvider::~CrostiniTerminalProvider() = default;

std::string CrostiniTerminalProvider::Label() {
  return crostini::FormatForUi(container_id_);
}

guest_os::GuestId CrostiniTerminalProvider::GuestId() {
  return container_id_;
}

bool CrostiniTerminalProvider::RecoveryRequired(int64_t display_id) {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  if (crostini_manager->IsUncleanStartup()) {
    ShowCrostiniRecoveryView(profile_, crostini::CrostiniUISurface::kAppList,
                             kTerminalSystemAppId, display_id, {},
                             base::DoNothing());
    return true;
  }
  return false;
}

std::string CrostiniTerminalProvider::PrepareCwd(storage::FileSystemURL url) {
  std::string cwd;
  CrostiniManager::RestartOptions options;
  auto* share_path = guest_os::GuestOsSharePath::GetForProfile(profile_);
  base::FilePath path;
  if (file_manager::util::ConvertFileSystemURLToPathInsideCrostini(
          profile_, url, &path)) {
    cwd = path.value();
    const auto& fs_id = url.mount_filesystem_id();
    auto mount_name =
        // TODO(b/217469540): Currently the default Crostini container gets
        // mounted in a different location to other Guest OS mounts, as we get
        // consistent file sharing across Guest OSs we can remove this special
        // case.
        (container_id_ == DefaultContainerId())
            ? file_manager::util::GetCrostiniMountPointName(profile_)
            : file_manager::util::GetGuestOsMountPointName(profile_,
                                                           container_id_);
    if (fs_id != mount_name &&
        !share_path->IsPathShared(container_id_.vm_name, url.path())) {
      // Path isn't already shared, so share it.
      options.share_paths.push_back(url.path());
    }
  } else {
    LOG(WARNING) << "Failed to parse: " << path << ". Not setting terminal cwd";
    return "";
  }
  // This completes async, but we don't wait for it since the terminal itself
  // also calls RestartCrostini and that'll get serialised, ensuring that this
  // call has completed before the share gets used.
  CrostiniManager::GetForProfile(profile_)->RestartCrostiniWithOptions(
      container_id_, std::move(options), base::DoNothing());
  return cwd;
}

}  // namespace crostini
