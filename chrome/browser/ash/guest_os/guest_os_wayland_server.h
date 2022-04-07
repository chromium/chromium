// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_WAYLAND_SERVER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_WAYLAND_SERVER_H_

#include "base/callback_forward.h"
#include "chrome/browser/ash/borealis/infra/expected.h"
#include "chromeos/dbus/vm_launch/launch.pb.h"

namespace guest_os {

class GuestOsWaylandServer {
 public:
  static void StartServer(
      const vm_tools::launch::StartWaylandServerRequest& request,
      base::OnceCallback<
          void(borealis::Expected<vm_tools::launch::StartWaylandServerResponse,
                                  std::string>)> response_callback);
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_WAYLAND_SERVER_H_
