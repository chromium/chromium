// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace regional_capabilities {
class RegionalCapabilitiesService;
}

namespace content {
class BrowserContext;
}

class Profile;

namespace regional_capabilities {
// Singleton that owns all RegionalCapabilitiesService and associates them with
// Profiles.
class RegionalCapabilitiesServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static regional_capabilities::RegionalCapabilitiesService* GetForProfile(
      Profile* profile);

  static RegionalCapabilitiesServiceFactory* GetInstance();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Returns whether the system profile is in a region where we can show a
  // search engine choice screen. If the profile is not a system profile, this
  // function crashes. Used for clients associated with system profiles (where
  // the service is not available, e.g. Profile Picker). Introduced for
  // controlling the launch of Chrome Desktop FRE Refresh project.
  //
  // TODO(crbug.com/486819807):  Remove once the feature is fully launched.
  static bool IsInSearchEngineChoiceScreenRegionForSystemProfile(
      Profile* profile);
#endif  // BUILDFLAG(IS_WINDOWS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

 private:
  friend base::NoDestructor<RegionalCapabilitiesServiceFactory>;

  RegionalCapabilitiesServiceFactory();
  ~RegionalCapabilitiesServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};
}  // namespace regional_capabilities

#endif  // CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_
