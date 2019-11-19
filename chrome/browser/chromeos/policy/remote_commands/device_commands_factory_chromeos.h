// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_CHROMEOS_H_

#include <memory>

#include "base/macros.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"

namespace policy {

class CRDHostDelegate;
class DeviceCloudPolicyManagerChromeOS;

class DeviceCommandsFactoryChromeOS : public RemoteCommandsFactory {
 public:
  explicit DeviceCommandsFactoryChromeOS(
      DeviceCloudPolicyManagerChromeOS* policy_manager);
  ~DeviceCommandsFactoryChromeOS() override;

  // RemoteCommandsFactory:
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      enterprise_management::RemoteCommand_Type type,
      RemoteCommandsService* service) override;

 private:
  DeviceCloudPolicyManagerChromeOS* policy_manager_;
  std::unique_ptr<CRDHostDelegate> crd_host_delegate_;

  CRDHostDelegate* GetCRDHostDelegate();

  DISALLOW_COPY_AND_ASSIGN(DeviceCommandsFactoryChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_CHROMEOS_H_
