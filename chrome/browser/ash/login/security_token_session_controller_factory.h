// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/login/security_token_session_controller.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace login {

class SecurityTokenSessionControllerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static SecurityTokenSessionController* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static SecurityTokenSessionControllerFactory* GetInstance();

 private:
  friend base::NoDestructor<SecurityTokenSessionControllerFactory>;

  SecurityTokenSessionControllerFactory();
  SecurityTokenSessionControllerFactory(
      const SecurityTokenSessionControllerFactory& other) = delete;
  SecurityTokenSessionControllerFactory& operator=(
      const SecurityTokenSessionControllerFactory& other) = delete;
  ~SecurityTokenSessionControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace login
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_FACTORY_H_
