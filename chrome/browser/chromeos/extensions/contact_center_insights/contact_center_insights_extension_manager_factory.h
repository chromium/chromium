// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTACT_CENTER_INSIGHTS_CONTACT_CENTER_INSIGHTS_EXTENSION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTACT_CENTER_INSIGHTS_CONTACT_CENTER_INSIGHTS_EXTENSION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace chromeos {

class ContactCenterInsightsExtensionManager;

class ContactCenterInsightsExtensionManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Retrieves the `ContactCenterInsightsExtensionManager` for the given
  // profile.
  static ContactCenterInsightsExtensionManager* GetForProfile(Profile* profile);

  // Retrieves the factory instance for the
  // `ContactCenterInsightsExtensionManager`.
  static ContactCenterInsightsExtensionManagerFactory* GetInstance();

  ContactCenterInsightsExtensionManagerFactory(
      const ContactCenterInsightsExtensionManagerFactory&) = delete;
  ContactCenterInsightsExtensionManagerFactory& operator=(
      const ContactCenterInsightsExtensionManagerFactory&) = delete;

 private:
  friend base::NoDestructor<ContactCenterInsightsExtensionManagerFactory>;

  ContactCenterInsightsExtensionManagerFactory();
  ~ContactCenterInsightsExtensionManagerFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTACT_CENTER_INSIGHTS_CONTACT_CENTER_INSIGHTS_EXTENSION_MANAGER_FACTORY_H_
