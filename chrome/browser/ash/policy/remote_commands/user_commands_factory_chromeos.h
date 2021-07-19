// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_USER_COMMANDS_FACTORY_CHROMEOS_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_USER_COMMANDS_FACTORY_CHROMEOS_H_

#include <memory>

#include "base/macros.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"

class Profile;

namespace policy {

class UserCommandsFactoryChromeOS : public RemoteCommandsFactory {
 public:
  explicit UserCommandsFactoryChromeOS(Profile* profile);
  ~UserCommandsFactoryChromeOS() override;

  // RemoteCommandsFactory:
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      enterprise_management::RemoteCommand_Type type,
      RemoteCommandsService* service) override;

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(UserCommandsFactoryChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_USER_COMMANDS_FACTORY_CHROMEOS_H_
