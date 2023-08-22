// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECOVERY_RECOVERY_INSTALL_GLOBAL_ERROR_FACTORY_H_
#define CHROME_BROWSER_RECOVERY_RECOVERY_INSTALL_GLOBAL_ERROR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class RecoveryInstallGlobalError;

// Singleton that owns all RecoveryInstallGlobalError and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated RecoveryInstallGlobalError.
class RecoveryInstallGlobalErrorFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of RecoveryInstallGlobalError associated with this
  // profile, creating one if none exists.
  static RecoveryInstallGlobalError* GetForProfile(Profile* profile);

  // Returns an instance of the RecoveryInstallGlobalErrorFactory singleton.
  static RecoveryInstallGlobalErrorFactory* GetInstance();

  RecoveryInstallGlobalErrorFactory(const RecoveryInstallGlobalErrorFactory&) =
      delete;
  RecoveryInstallGlobalErrorFactory& operator=(
      const RecoveryInstallGlobalErrorFactory&) = delete;

 private:
  friend base::NoDestructor<RecoveryInstallGlobalErrorFactory>;

  RecoveryInstallGlobalErrorFactory();
  ~RecoveryInstallGlobalErrorFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_RECOVERY_RECOVERY_INSTALL_GLOBAL_ERROR_FACTORY_H_
