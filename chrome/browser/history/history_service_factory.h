// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_HISTORY_HISTORY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"

class Profile;

namespace history {
class HistoryService;
}

// Singleton that owns all HistoryService and associates them with
// Profiles.
class HistoryServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static history::HistoryService* GetForProfile(Profile* profile,
                                                ServiceAccessType sat);

  static history::HistoryService* GetForProfileIfExists(Profile* profile,
                                                        ServiceAccessType sat);

  static history::HistoryService* GetForProfileWithoutCreating(
      Profile* profile);

  static HistoryServiceFactory* GetInstance();

  // In the testing profile, we often clear the history before making a new
  // one. This takes care of that work. It should only be used in tests.
  // Note: This does not do any cleanup; it only destroys the service. The
  // calling test is expected to do the cleanup before calling this function.
  static void ShutdownForProfile(Profile* profile);

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend base::NoDestructor<HistoryServiceFactory>;

  HistoryServiceFactory();
  ~HistoryServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_SERVICE_FACTORY_H_
