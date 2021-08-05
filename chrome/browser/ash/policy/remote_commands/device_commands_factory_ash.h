// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_ASH_H_

#include <memory>

#include "base/macros.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"

namespace policy {

class CRDHostDelegate;
class CrdLockoutStrategy;
class DeviceCloudPolicyManagerAsh;

class DeviceCommandsFactoryAsh : public RemoteCommandsFactory {
 public:
  explicit DeviceCommandsFactoryAsh(
      DeviceCloudPolicyManagerAsh* policy_manager);
  ~DeviceCommandsFactoryAsh() override;

  // RemoteCommandsFactory:
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      enterprise_management::RemoteCommand_Type type,
      RemoteCommandsService* service) override;

 private:
  DeviceCloudPolicyManagerAsh* policy_manager_;
  // Note: This is used by |crd_host_delegate_| so it must always outlive
  // |crd_host_delegate_|.
  std::unique_ptr<CrdLockoutStrategy> crd_lockout_strategy_;
  std::unique_ptr<CRDHostDelegate> crd_host_delegate_;

  CRDHostDelegate* GetCRDHostDelegate();
  CrdLockoutStrategy* GetCrdLockoutStrategy();

  DISALLOW_COPY_AND_ASSIGN(DeviceCommandsFactoryAsh);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMANDS_FACTORY_ASH_H_
