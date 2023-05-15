// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_ASH_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_screenshot_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"

namespace policy {

class DeviceCloudPolicyManagerAsh;

class DeviceCommandsFactoryAsh : public RemoteCommandsFactory {
 public:
  explicit DeviceCommandsFactoryAsh(
      DeviceCloudPolicyManagerAsh* policy_manager);

  DeviceCommandsFactoryAsh(const DeviceCommandsFactoryAsh&) = delete;
  DeviceCommandsFactoryAsh& operator=(const DeviceCommandsFactoryAsh&) = delete;

  ~DeviceCommandsFactoryAsh() override;

  // RemoteCommandsFactory:
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      enterprise_management::RemoteCommand_Type type,
      RemoteCommandsService* service) override;

  static void set_commands_for_testing(bool device_commands_test);

 private:
  // TODO(b/269432279): Consider removing when test uses a local upload server
  static bool device_commands_test_;

  raw_ptr<DeviceCloudPolicyManagerAsh, ExperimentalAsh> policy_manager_;
  std::unique_ptr<DeviceCommandStartCrdSessionJob::Delegate> crd_host_delegate_;

  DeviceCommandStartCrdSessionJob::Delegate* GetCrdHostDelegate();
  std::unique_ptr<DeviceCommandScreenshotJob::Delegate>
  CreateScreenshotDelegate();
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_ASH_H_
