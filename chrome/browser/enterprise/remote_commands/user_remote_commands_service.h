// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/remote_commands/user_remote_commands_service_base.h"

#include "base/memory/raw_ptr.h"

#ifndef CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_SERVICE_H_

class Profile;

namespace enterprise_commands {

class UserRemoteCommandsService : public policy::UserRemoteCommandsServiceBase {
 public:
  explicit UserRemoteCommandsService(Profile* profile);
  UserRemoteCommandsService(const UserRemoteCommandsService&) = delete;
  UserRemoteCommandsService& operator=(const UserRemoteCommandsService&) =
      delete;
  ~UserRemoteCommandsService() override;

 private:
  // policy::UserRemoteCommandsServiceBase
  invalidation::ProfileInvalidationProvider* GetInvalidationProvider() override;
  std::unique_ptr<policy::RemoteCommandsFactory> GetFactory() override;

  raw_ptr<Profile> profile_;
};

}  // namespace enterprise_commands

#endif  // CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_SERVICE_H_
