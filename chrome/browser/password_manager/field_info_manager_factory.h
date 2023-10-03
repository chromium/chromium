// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_FIELD_INFO_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_FIELD_INFO_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace password_manager {
class FieldInfoManager;
}

class FieldInfoManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static FieldInfoManagerFactory* GetInstance();

  // Returns the FieldInfoManager associated with |profile|.
  // This may be nullptr for an incognito profile.
  static password_manager::FieldInfoManager* GetForProfile(Profile* profile);

  FieldInfoManagerFactory(const FieldInfoManagerFactory&) = delete;
  FieldInfoManagerFactory& operator=(const FieldInfoManagerFactory&) = delete;

 private:
  friend base::NoDestructor<FieldInfoManagerFactory>;

  FieldInfoManagerFactory();
  ~FieldInfoManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_FIELD_INFO_MANAGER_FACTORY_H_
