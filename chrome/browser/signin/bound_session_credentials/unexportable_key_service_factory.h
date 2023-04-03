// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace unexportable_keys {
class UnexportableKeyService;
}

class UnexportableKeyServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns nullptr if unexportable key provider is not supported by the
  // platform.
  static unexportable_keys::UnexportableKeyService* GetForProfile(
      Profile* profile);

  static UnexportableKeyServiceFactory* GetInstance();

  UnexportableKeyServiceFactory(const UnexportableKeyServiceFactory&) = delete;
  UnexportableKeyServiceFactory& operator=(
      const UnexportableKeyServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<UnexportableKeyServiceFactory>;

  UnexportableKeyServiceFactory();
  ~UnexportableKeyServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_SERVICE_FACTORY_H_
