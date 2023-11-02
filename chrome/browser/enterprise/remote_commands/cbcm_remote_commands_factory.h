// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_CBCM_REMOTE_COMMANDS_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_CBCM_REMOTE_COMMANDS_FACTORY_H_

#include "components/policy/core/common/remote_commands/remote_commands_factory.h"

namespace enterprise_commands {

// A remote commands factory meant to be used with RemoteCommandService on
// Chrome Browser Cloud Management-enrolled browsers.
class CBCMRemoteCommandsFactory : public policy::RemoteCommandsFactory {
 public:
  CBCMRemoteCommandsFactory() = default;
  ~CBCMRemoteCommandsFactory() override = default;

  // RemoteCommandsFactory:
  std::unique_ptr<policy::RemoteCommandJob> BuildJobForType(
      enterprise_management::RemoteCommand_Type type,
      policy::RemoteCommandsService* service) override;
};

}  // namespace enterprise_commands

#endif  // CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_CBCM_REMOTE_COMMANDS_FACTORY_H_
