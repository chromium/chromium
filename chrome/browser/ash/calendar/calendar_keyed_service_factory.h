// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CALENDAR_CALENDAR_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CALENDAR_CALENDAR_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class CalendarKeyedService;

// Factory class for browser context keyed calendar services. Only builds
// service instance for `user->HasGaiaAccount()` and returns `nullptr` for the
// other user types.
class CalendarKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static CalendarKeyedServiceFactory* GetInstance();

  CalendarKeyedService* GetService(content::BrowserContext* context);

 protected:
  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override {}

 private:
  friend base::NoDestructor<CalendarKeyedServiceFactory>;

  CalendarKeyedServiceFactory();
  CalendarKeyedServiceFactory(const CalendarKeyedServiceFactory& other) =
      delete;
  CalendarKeyedServiceFactory& operator=(
      const CalendarKeyedServiceFactory& other) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CALENDAR_CALENDAR_KEYED_SERVICE_FACTORY_H_
