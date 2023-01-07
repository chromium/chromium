// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_CHROME_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_CHROME_MANAGEMENT_SERVICE_H_

#include <cstdint>
#include <memory>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace base {
class CommandLine;
}  // namespace base

namespace enterprise_connectors {

class MojoHelper;

// Drives the key rotation operation for the ChromeManagementService
// process.
class ChromeManagementService {
 public:
  ChromeManagementService();
  ~ChromeManagementService();

  // Executes the command specified by `command_line` for the current
  // process. `pipe_name`is the name of the pipe to connect to. This
  // function returns the success/failure status of the key rotation.
  int Run(const base::CommandLine* command_line, uint64_t pipe_name);

 private:
  friend class ChromeManagementServiceTest;

  // Callback to the CheckBinaryPermissions function.
  using PermissionsCallback = base::OnceCallback<bool()>;

  // Callback to the StartRotation function.
  using RotationCallback =
      base::OnceCallback<int(const base::CommandLine* command_line)>;

  // Strictly used for testing and allows mocking the permissions check
  // and starting the key rotation. The `permissions_callback` is the
  // callback to the CheckBinaryPermissions function, the
  // `rotation_callback` is a callback to the StartRotation function,
  // and the `mojo_helper` is the helper that issues mojo APIs.
  ChromeManagementService(PermissionsCallback permissions_callback,
                          RotationCallback rotation_callback,
                          std::unique_ptr<MojoHelper> mojo_helper);

  // Starts the key rotation using the `command_line` for the current process.
  //
  // This function will block until the boolean result of the key rotation
  // call is returned. This function is not meant to be called from the chrome
  // browser but from a background utility process that does not block the user
  // in the browser.
  int StartRotation(const base::CommandLine* command_line);

  PermissionsCallback permissions_callback_;
  RotationCallback rotation_callback_;

  // Remote url loader factory bound to the url loader from the browser
  // process.
  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory_;

  // Helper class responsible for issuing mojo APIs.
  std::unique_ptr<MojoHelper> mojo_helper_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_CHROME_MANAGEMENT_SERVICE_H_
