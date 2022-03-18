// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_CHROME_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_CHROME_MANAGEMENT_SERVICE_H_

#include <cstdint>

#include "base/callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

namespace base {
class CommandLine;
}  // namespace base

namespace enterprise_connectors {

// Drives the key rotation operation for the ChromeManagementService
// process.
class ChromeManagementService {
 public:
  ChromeManagementService();
  ~ChromeManagementService();

  // Executes the command specified by `command_line` for the current
  // process. `pipe_name`is the name of the pipe to connect to. This
  // function returns the result of the key rotation.
  int Run(const base::CommandLine* command_line, uint64_t pipe_name);

 private:
  friend class ChromeManagementServiceTest;

  // Callback to the CheckBinaryPermissions function.
  using PermissionsCallback = base::OnceCallback<bool()>;

  // Callback to the StartRotation function.
  using RotationCallback = base::OnceCallback<int(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          remote_url_loader_factory,
      const base::CommandLine* command_line)>;

  // Strictly used for testing and allows mocking the permissions check
  // and starting the key rotation. The `permissions_callback` is the
  // callback to the CheckBinaryPermissions function, and the
  // `rotation_callback` is a callback to the StartRotation function.
  ChromeManagementService(PermissionsCallback permissions_callback,
                          RotationCallback rotation_callback);

  PermissionsCallback permissions_callback_;
  RotationCallback rotation_callback_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_CHROME_MANAGEMENT_SERVICE_H_
