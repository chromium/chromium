// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_DATA_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SESSIONS_SESSION_DATA_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class SessionDataService;
class Profile;

class SessionDataServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SessionDataService* GetForProfile(Profile* profile);

  static SessionDataServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<SessionDataServiceFactory>;

  SessionDataServiceFactory();
  ~SessionDataServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_DATA_SERVICE_FACTORY_H_
