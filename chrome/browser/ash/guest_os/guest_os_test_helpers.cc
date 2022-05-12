// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_test_helpers.h"
#include "chrome/browser/ash/guest_os/public/types.h"

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TEST_HELPERS_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TEST_HELPERS_H_

namespace guest_os {

MockMountProvider::MockMountProvider()
    : profile_(nullptr), container_id_(crostini::ContainerId::GetDefault()) {}

MockMountProvider::MockMountProvider(Profile* profile,
                                     crostini::ContainerId container_id)
    : profile_(profile), container_id_(container_id) {}

std::string MockMountProvider::DisplayName() {
  return "Ptery";
}

Profile* MockMountProvider::profile() {
  return profile_;
}

crostini::ContainerId MockMountProvider::ContainerId() {
  return container_id_;
}

VmType MockMountProvider::vm_type() {
  return VmType::ApplicationList_VmType_PLUGIN_VM;
}

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TEST_HELPERS_H_
