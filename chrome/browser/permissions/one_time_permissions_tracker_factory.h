// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}
class OneTimePermissionsTracker;

class OneTimePermissionsTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  OneTimePermissionsTrackerFactory(const OneTimePermissionsTrackerFactory&) =
      delete;
  OneTimePermissionsTrackerFactory& operator=(
      const OneTimePermissionsTrackerFactory&) = delete;

  static OneTimePermissionsTracker* GetForBrowserContext(
      content::BrowserContext* context);
  static OneTimePermissionsTrackerFactory* GetInstance();

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<OneTimePermissionsTrackerFactory>;

  OneTimePermissionsTrackerFactory();
  ~OneTimePermissionsTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_FACTORY_H_
