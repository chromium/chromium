// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"

class Profile;

namespace enterprise_commands {

class UserRemoteCommandsFactory : public policy::RemoteCommandsFactory {
 public:
  explicit UserRemoteCommandsFactory(Profile* profile);
  UserRemoteCommandsFactory(const UserRemoteCommandsFactory&) = delete;
  UserRemoteCommandsFactory& operator=(const UserRemoteCommandsFactory&) =
      delete;
  ~UserRemoteCommandsFactory() override;

  // RemoteCommandsFactory:
  std::unique_ptr<policy::RemoteCommandJob> BuildJobForType(
      enterprise_management::RemoteCommand_Type type,
      policy::RemoteCommandsService* service) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace enterprise_commands

#endif  // CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_FACTORY_H_
