// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_TEST_HELPERS_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_TEST_HELPERS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "chrome/browser/ash/guest_os/public/types.h"

namespace guest_os {
class MockMountProvider : public GuestOsMountProvider {
 public:
  MockMountProvider();
  MockMountProvider(Profile* profile, guest_os::GuestId container_id);

  std::string DisplayName() override;
  Profile* profile() override;
  guest_os::GuestId GuestId() override;

  VmType vm_type() override;
  void Prepare(base::OnceCallback<
               void(bool success, int cid, int port, base::FilePath homedir)>
                   callback) override;

  std::unique_ptr<GuestOsFileWatcher> CreateFileWatcher(
      base::FilePath mount_path,
      base::FilePath relative_path) override;

  raw_ptr<Profile> profile_;
  guest_os::GuestId container_id_;
};
}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_TEST_HELPERS_H_
