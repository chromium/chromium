// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_EXIT_TYPE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SESSIONS_EXIT_TYPE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ExitTypeService;
class Profile;

// BrowserContextKeyedServiceFactory used to create ExitTypeService.
class ExitTypeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ExitTypeService* GetForProfile(Profile* profile);

  static ExitTypeServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ExitTypeServiceFactory>;

  ExitTypeServiceFactory();
  ~ExitTypeServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SESSIONS_EXIT_TYPE_SERVICE_FACTORY_H_
