// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace feature_guide {
class FeatureNotificationGuideService;

// A factory to create one unique FeatureNotificationGuideService.
class FeatureNotificationGuideServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static FeatureNotificationGuideServiceFactory* GetInstance();
  static FeatureNotificationGuideService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<
      FeatureNotificationGuideServiceFactory>;

  FeatureNotificationGuideServiceFactory();
  ~FeatureNotificationGuideServiceFactory() override = default;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_FACTORY_H_
