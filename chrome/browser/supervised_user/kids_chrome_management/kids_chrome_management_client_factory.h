// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_CHROME_MANAGEMENT_CLIENT_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_CHROME_MANAGEMENT_CLIENT_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class KidsChromeManagementClient;

// Singleton that owns all KidsChromeManagementClient instances and associates
// them with Profiles. Listens for the Profile's destruction
// notification and cleans up the associated KidsChromeManagementClient.
class KidsChromeManagementClientFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static KidsChromeManagementClient* GetForBrowserContext(Profile* profile);

  static KidsChromeManagementClientFactory* GetInstance();

 private:
  friend class base::NoDestructor<KidsChromeManagementClientFactory>;

  KidsChromeManagementClientFactory();
  ~KidsChromeManagementClientFactory() override;

  // The context parameter is guaranteed to be a Profile* because this is what
  // GetForBrowserContext receives. It's only declared as a BrowserContext*
  // because this method is overriding another one from the parent class.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(KidsChromeManagementClientFactory);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_CHROME_MANAGEMENT_CLIENT_FACTORY_H_
