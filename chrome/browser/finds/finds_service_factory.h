// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_FINDS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FINDS_FINDS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace finds {
class FindsService;

// Service factory to provide `FindsService` for a given profile.
class FindsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static FindsServiceFactory* GetInstance();
  static FindsService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<FindsServiceFactory>;

  FindsServiceFactory();
  ~FindsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace finds

#endif  // CHROME_BROWSER_FINDS_FINDS_SERVICE_FACTORY_H_
