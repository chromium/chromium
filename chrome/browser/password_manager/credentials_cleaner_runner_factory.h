// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIALS_CLEANER_RUNNER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIALS_CLEANER_RUNNER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace password_manager {
class CredentialsCleanerRunner;
}  // namespace password_manager

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

// Creates instances of CredentialsCleanerRunner per Profile.
class CredentialsCleanerRunnerFactory : public ProfileKeyedServiceFactory {
 public:
  static CredentialsCleanerRunnerFactory* GetInstance();
  static password_manager::CredentialsCleanerRunner* GetForProfile(
      Profile* profile);

 private:
  friend class base::NoDestructor<CredentialsCleanerRunnerFactory>;

  CredentialsCleanerRunnerFactory();
  ~CredentialsCleanerRunnerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIALS_CLEANER_RUNNER_FACTORY_H_
