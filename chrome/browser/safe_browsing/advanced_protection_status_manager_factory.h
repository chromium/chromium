// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace safe_browsing {

class AdvancedProtectionStatusManager;

// Responsible for keeping track of advanced protection status of the sign-in
// profile.
class AdvancedProtectionStatusManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static AdvancedProtectionStatusManager* GetForProfile(Profile* profile);

  static AdvancedProtectionStatusManagerFactory* GetInstance();

  AdvancedProtectionStatusManagerFactory(
      const AdvancedProtectionStatusManagerFactory&) = delete;
  AdvancedProtectionStatusManagerFactory& operator=(
      const AdvancedProtectionStatusManagerFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      AdvancedProtectionStatusManagerFactory>;

  AdvancedProtectionStatusManagerFactory();
  ~AdvancedProtectionStatusManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_FACTORY_H_
