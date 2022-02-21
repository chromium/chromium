// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_H_

#include <string>

namespace guest_os {

class GuestOsMountProvider {
 public:
  virtual ~GuestOsMountProvider() = default;

  // The localised name to show in UI elements such as the files app sidebar.
  virtual std::string DisplayName() = 0;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_H_
