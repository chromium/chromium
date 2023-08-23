// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

class PrintJobHistoryService;

// Singleton that owns all PrintJobHistoryServices and associates them with
// Profiles.
class PrintJobHistoryServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the PrintJobHistoryService for |context|, creating it if it is not
  // yet created.
  static PrintJobHistoryService* GetForBrowserContext(
      content::BrowserContext* context);

  static PrintJobHistoryServiceFactory* GetInstance();

  PrintJobHistoryServiceFactory(const PrintJobHistoryServiceFactory&) = delete;
  PrintJobHistoryServiceFactory& operator=(
      const PrintJobHistoryServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PrintJobHistoryServiceFactory>;

  PrintJobHistoryServiceFactory();
  ~PrintJobHistoryServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_FACTORY_H_
