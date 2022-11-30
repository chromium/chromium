// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_LAUNCHER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_LAUNCHER_H_

#include "chrome/browser/ash/borealis/infra/expected.h"
#include "chromeos/ash/components/dbus/vm_launch/launch.pb.h"

namespace guest_os::launcher {

using ResponseType =
    borealis::Expected<vm_tools::launch::EnsureVmLaunchedResponse, std::string>;

using LaunchCallback = base::OnceCallback<void(ResponseType)>;

void EnsureLaunched(const vm_tools::launch::EnsureVmLaunchedRequest& request,
                    LaunchCallback response_callback);

}  // namespace guest_os::launcher

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_LAUNCHER_H_
