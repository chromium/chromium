// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_FIELD_INFO_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_FIELD_INFO_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace password_manager {
class FieldInfoManager;
}

class FieldInfoManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static FieldInfoManagerFactory* GetInstance();

  // Returns the FieldInfoManager associated with |context|.
  // This may be nullptr for an incognito |context|.
  static password_manager::FieldInfoManager* GetForBrowserContext(
      content::BrowserContext* context);

  FieldInfoManagerFactory(const FieldInfoManagerFactory&) = delete;
  FieldInfoManagerFactory& operator=(const FieldInfoManagerFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<FieldInfoManagerFactory>;

  FieldInfoManagerFactory();
  ~FieldInfoManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_FIELD_INFO_MANAGER_FACTORY_H_
