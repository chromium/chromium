// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ChromePasswordChangeService;

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

// Creates instances of PasswordChangeServiceInterface per Profile.
class PasswordChangeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PasswordChangeServiceFactory* GetInstance();
  static ChromePasswordChangeService* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<PasswordChangeServiceFactory>;

  PasswordChangeServiceFactory();
  ~PasswordChangeServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_SERVICE_FACTORY_H_
