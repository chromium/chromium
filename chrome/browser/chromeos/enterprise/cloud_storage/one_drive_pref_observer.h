// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ENTERPRISE_CLOUD_STORAGE_ONE_DRIVE_PREF_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_ENTERPRISE_CLOUD_STORAGE_ONE_DRIVE_PREF_OBSERVER_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace chromeos::cloud_storage {

// This factory reacts to profile creation and instantiates profile-keyed
// services that set up observers for prefs related to Microsoft.
// The keyed service will run on the same side (ash / lacros) as the
// odfs (OneDrive file system) extension.
class OneDrivePrefObserverFactory : public ProfileKeyedServiceFactory {
 public:
  static OneDrivePrefObserverFactory* GetInstance();

  OneDrivePrefObserverFactory(const OneDrivePrefObserverFactory&) = delete;
  OneDrivePrefObserverFactory& operator=(const OneDrivePrefObserverFactory&) =
      delete;

 private:
  friend base::NoDestructor<OneDrivePrefObserverFactory>;

  OneDrivePrefObserverFactory();
  ~OneDrivePrefObserverFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace chromeos::cloud_storage

#endif  // CHROME_BROWSER_CHROMEOS_ENTERPRISE_CLOUD_STORAGE_ONE_DRIVE_PREF_OBSERVER_H_
