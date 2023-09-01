// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace metrics {

class DesktopProfileSessionDurationsService;

// Singleton that owns all DesktopProfileSessionDurationsServices and associates
// them with BrowserContexts. Listens for the BrowserContext's destruction
// notification and cleans up the associated
// DesktopProfileSessionDurationsServices.
class DesktopProfileSessionDurationsServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given
  // BrowserContext.
  static DesktopProfileSessionDurationsService* GetForBrowserContext(
      content::BrowserContext* context);

  static DesktopProfileSessionDurationsServiceFactory* GetInstance();

  DesktopProfileSessionDurationsServiceFactory(
      const DesktopProfileSessionDurationsServiceFactory&) = delete;
  DesktopProfileSessionDurationsServiceFactory& operator=(
      const DesktopProfileSessionDurationsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<DesktopProfileSessionDurationsServiceFactory>;

  DesktopProfileSessionDurationsServiceFactory();
  ~DesktopProfileSessionDurationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
