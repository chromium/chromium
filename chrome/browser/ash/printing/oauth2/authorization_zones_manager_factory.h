// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONES_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONES_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace ash {
namespace printing {
namespace oauth2 {

class AuthorizationZonesManager;

class AuthorizationZonesManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static AuthorizationZonesManagerFactory* GetInstance();
  static AuthorizationZonesManager* GetForBrowserContext(
      content::BrowserContext* context);

  AuthorizationZonesManagerFactory(const AuthorizationZonesManagerFactory&) =
      delete;
  AuthorizationZonesManagerFactory& operator=(
      const AuthorizationZonesManagerFactory&) = delete;

 private:
  friend base::NoDestructor<AuthorizationZonesManagerFactory>;

  AuthorizationZonesManagerFactory();
  ~AuthorizationZonesManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONES_MANAGER_FACTORY_H_
