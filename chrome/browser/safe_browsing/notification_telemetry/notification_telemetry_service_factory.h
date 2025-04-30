// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace safe_browsing {

class NotificationTelemetryService;

// Singleton that owns NotificationTelemetryService objects, one for
// each active Profile. It listens to profile destroy events and destroys its
// associated service. It returns nullptr if the profile is in the Incognito
// mode.
class NotificationTelemetryServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static NotificationTelemetryService* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static NotificationTelemetryServiceFactory* GetInstance();

  NotificationTelemetryServiceFactory(
      const NotificationTelemetryServiceFactory&) = delete;
  NotificationTelemetryServiceFactory& operator=(
      const NotificationTelemetryServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<NotificationTelemetryServiceFactory>;

  NotificationTelemetryServiceFactory();
  ~NotificationTelemetryServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_SERVICE_FACTORY_H_
