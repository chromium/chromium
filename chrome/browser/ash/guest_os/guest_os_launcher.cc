// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_launcher.h"

namespace guest_os::launcher {

void EnsureLaunched(const vm_tools::launch::EnsureVmLaunchedRequest& request,
                    LaunchCallback response_callback) {
  std::move(response_callback).Run(ResponseType::Unexpected("Not Implemented"));
}

}  // namespace guest_os::launcher
