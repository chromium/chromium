// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class ExtensionTelemetryService;

// Singleton that produces ExtensionTelemetryService objects, one for each
// active Profile.
class ExtensionTelemetryServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  // Returns nullptr if the profile is in Incognito/Guest mode
  static ExtensionTelemetryService* GetForProfile(Profile* profile);

  static ExtensionTelemetryServiceFactory* GetInstance();

  ExtensionTelemetryServiceFactory(const ExtensionTelemetryServiceFactory&) =
      delete;
  ExtensionTelemetryServiceFactory& operator=(
      const ExtensionTelemetryServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<ExtensionTelemetryServiceFactory>;

  ExtensionTelemetryServiceFactory();
  ~ExtensionTelemetryServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_FACTORY_H_
