// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_LAUNCHER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_LAUNCHER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chromeos/ash/components/dbus/vm_launch/launch.pb.h"

class Profile;
namespace guest_os::launcher {

using ResponseType =
    base::expected<vm_tools::launch::EnsureVmLaunchedResponse, std::string>;

using LaunchCallback = base::OnceCallback<void(ResponseType)>;

using SuccessCallback =
    base::OnceCallback<void(bool success, const std::string& failure_reason)>;

// Launched the VM if necessary, then runs the callback on both success and
// failure.
void EnsureLaunched(const vm_tools::launch::EnsureVmLaunchedRequest& request,
                    LaunchCallback response_callback);

// Asynchronously launches an app as specified by its registration, in the
// specified guest, then runs the callback.
void LaunchApplication(
    Profile* profile,
    const guest_os::GuestId& guest_id,
    guest_os::GuestOsRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<std::string>& files,
    SuccessCallback callback);

}  // namespace guest_os::launcher

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_LAUNCHER_H_
