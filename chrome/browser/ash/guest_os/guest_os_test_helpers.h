// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_TEST_HELPERS_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_TEST_HELPERS_H_

#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "chrome/browser/ash/guest_os/public/types.h"

namespace guest_os {
class MockMountProvider : public GuestOsMountProvider {
 public:
  MockMountProvider();
  MockMountProvider(Profile* profile, crostini::ContainerId container_id);

  std::string DisplayName() override;
  Profile* profile() override;
  crostini::ContainerId ContainerId() override;

  VmType vm_type() override;

  Profile* profile_;
  crostini::ContainerId container_id_;
};
}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_TEST_HELPERS_H_
