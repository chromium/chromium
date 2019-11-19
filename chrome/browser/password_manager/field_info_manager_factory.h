// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_FIELD_INFO_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_FIELD_INFO_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace password_manager {
class FieldInfoManager;
}

class FieldInfoManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static FieldInfoManagerFactory* GetInstance();

  // Returns the FieldInfoManager associated with |context|.
  // This may be nullptr for an incognito |context|.
  static password_manager::FieldInfoManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<FieldInfoManagerFactory>;

  FieldInfoManagerFactory();
  ~FieldInfoManagerFactory() override;
  FieldInfoManagerFactory(const FieldInfoManagerFactory&) = delete;
  FieldInfoManagerFactory& operator=(const FieldInfoManagerFactory&) = delete;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_FIELD_INFO_MANAGER_FACTORY_H_
