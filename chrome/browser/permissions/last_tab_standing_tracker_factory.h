// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}
class LastTabStandingTracker;

class LastTabStandingTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  LastTabStandingTrackerFactory(const LastTabStandingTrackerFactory&) = delete;
  LastTabStandingTrackerFactory& operator=(
      const LastTabStandingTrackerFactory&) = delete;

  static LastTabStandingTracker* GetForBrowserContext(
      content::BrowserContext* context);
  static LastTabStandingTrackerFactory* GetInstance();

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend struct base::DefaultSingletonTraits<LastTabStandingTrackerFactory>;

  LastTabStandingTrackerFactory();
  ~LastTabStandingTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_FACTORY_H_
