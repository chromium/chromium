// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/ash/login/security_token_session_controller.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {
namespace login {

class SecurityTokenSessionControllerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static SecurityTokenSessionController* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static SecurityTokenSessionControllerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      SecurityTokenSessionControllerFactory>;

  SecurityTokenSessionControllerFactory();
  SecurityTokenSessionControllerFactory(
      const SecurityTokenSessionControllerFactory& other) = delete;
  SecurityTokenSessionControllerFactory& operator=(
      const SecurityTokenSessionControllerFactory& other) = delete;
  ~SecurityTokenSessionControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace login
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_FACTORY_H_
