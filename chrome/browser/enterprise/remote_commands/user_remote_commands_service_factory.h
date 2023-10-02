// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace enterprise_commands {

class UserRemoteCommandsService;

class UserRemoteCommandsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static UserRemoteCommandsServiceFactory* GetInstance();
  static UserRemoteCommandsService* GetForProfile(Profile* profile);

  UserRemoteCommandsServiceFactory(const UserRemoteCommandsServiceFactory&) =
      delete;
  UserRemoteCommandsServiceFactory& operator=(
      const UserRemoteCommandsServiceFactory&) = delete;
  ~UserRemoteCommandsServiceFactory() override;

 private:
  friend base::NoDestructor<UserRemoteCommandsServiceFactory>;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  UserRemoteCommandsServiceFactory();
};

}  // namespace enterprise_commands

#endif  // CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_USER_REMOTE_COMMANDS_SERVICE_FACTORY_H_
