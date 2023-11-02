// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_mount_provider.h"

#include "ash/components/arc/arc_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "ui/base/l10n/l10n_util.h"

namespace arc {

namespace {

constexpr uint32_t kVsockPort = 7780;

}  // namespace

ArcMountProvider::ArcMountProvider(Profile* profile, int cid)
    : profile_(profile), cid_(cid) {}

ArcMountProvider::~ArcMountProvider() = default;

// GuestOsMountProvider overrides
Profile* ArcMountProvider::profile() {
  return profile_;
}

std::string ArcMountProvider::DisplayName() {
  return l10n_util::GetStringUTF8(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_ANDROID_FILES);
}

guest_os::GuestId ArcMountProvider::GuestId() {
  return {guest_os::VmType::ARCVM, kArcVmName, ""};
}

guest_os::VmType ArcMountProvider::vm_type() {
  return guest_os::VmType::ARCVM;
}

void ArcMountProvider::Prepare(PrepareCallback callback) {
  std::move(callback).Run(true, cid_, kVsockPort, base::FilePath());
}

std::unique_ptr<guest_os::GuestOsFileWatcher>
ArcMountProvider::CreateFileWatcher(base::FilePath mount_path,
                                    base::FilePath relative_path) {
  return nullptr;
}

}  // namespace arc
