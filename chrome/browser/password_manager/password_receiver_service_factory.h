// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_RECEIVER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_RECEIVER_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace password_manager {
class PasswordReceiverService;
}

namespace content {
class BrowserContext;
}

class Profile;

// Creates instances of PasswordReceiverService per Profile.
class PasswordReceiverServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PasswordReceiverServiceFactory* GetInstance();
  static password_manager::PasswordReceiverService* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<PasswordReceiverServiceFactory>;

  PasswordReceiverServiceFactory();
  ~PasswordReceiverServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_RECEIVER_SERVICE_FACTORY_H_
