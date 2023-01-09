// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_LINUX_KEY_ROTATION_COMMAND_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_LINUX_KEY_ROTATION_COMMAND_H_

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace enterprise_connectors {

class LinuxKeyRotationCommand : public KeyRotationCommand {
 public:
  explicit LinuxKeyRotationCommand(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~LinuxKeyRotationCommand() override;

  // KeyRotationCommand:
  void Trigger(const Params& params, Callback callback) override;

 private:
  friend class LinuxKeyRotationCommandTest;

  // Callback to the Launch Process function.
  using LaunchCallback =
      base::RepeatingCallback<base::Process(const base::CommandLine&,
                                            const base::LaunchOptions&)>;

  // Strictly used for testing and allows mocking the launched process.
  // The `launch_callback` is a callback to the
  // LaunchChromeManagementServiceBinary function. `url_loader_factory`
  // is the test shared url loader factory.
  LinuxKeyRotationCommand(
      LaunchCallback launch_callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  LaunchCallback launch_callback_;

  // The `shared url loader factory` is the url loader factory
  // received from the browser process.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_LINUX_KEY_ROTATION_COMMAND_H_
