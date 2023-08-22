// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SESSION_TRACKER_FACTORY_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SESSION_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace guest_os {

class GuestOsSessionTracker;

class GuestOsSessionTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static GuestOsSessionTracker* GetForProfile(Profile* profile);
  static GuestOsSessionTrackerFactory* GetInstance();

  GuestOsSessionTrackerFactory(const GuestOsSessionTrackerFactory&) = delete;
  GuestOsSessionTrackerFactory& operator=(const GuestOsSessionTrackerFactory&) =
      delete;

 private:
  friend class base::NoDestructor<GuestOsSessionTrackerFactory>;

  GuestOsSessionTrackerFactory();
  ~GuestOsSessionTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SESSION_TRACKER_FACTORY_H_
