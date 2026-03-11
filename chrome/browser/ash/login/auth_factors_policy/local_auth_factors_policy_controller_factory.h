// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_AUTH_FACTORS_POLICY_LOCAL_AUTH_FACTORS_POLICY_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_AUTH_FACTORS_POLICY_LOCAL_AUTH_FACTORS_POLICY_CONTROLLER_FACTORY_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {

template <typename T>
class NoDestructor;

}  // namespace base

namespace ash {

class LocalAuthFactorsPolicyController;

class LocalAuthFactorsPolicyControllerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static LocalAuthFactorsPolicyController* GetForProfile(Profile* profile);
  static LocalAuthFactorsPolicyControllerFactory* GetInstance();

  LocalAuthFactorsPolicyControllerFactory(
      const LocalAuthFactorsPolicyControllerFactory&) = delete;
  LocalAuthFactorsPolicyControllerFactory& operator=(
      const LocalAuthFactorsPolicyControllerFactory&) = delete;

 private:
  friend class base::NoDestructor<LocalAuthFactorsPolicyControllerFactory>;

  LocalAuthFactorsPolicyControllerFactory();
  ~LocalAuthFactorsPolicyControllerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_AUTH_FACTORS_POLICY_LOCAL_AUTH_FACTORS_POLICY_CONTROLLER_FACTORY_H_
