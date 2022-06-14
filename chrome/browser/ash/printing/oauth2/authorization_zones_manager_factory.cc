// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {
namespace printing {
namespace oauth2 {

// static
AuthorizationZonesManagerFactory*
AuthorizationZonesManagerFactory::GetInstance() {
  static base::NoDestructor<AuthorizationZonesManagerFactory> factory;
  return factory.get();
}

// static
AuthorizationZonesManager*
AuthorizationZonesManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AuthorizationZonesManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AuthorizationZonesManagerFactory::AuthorizationZonesManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "AuthorizationZonesManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {}

AuthorizationZonesManagerFactory::~AuthorizationZonesManagerFactory() = default;

KeyedService* AuthorizationZonesManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return AuthorizationZonesManager::Create(Profile::FromBrowserContext(context))
      .release();
}

content::BrowserContext*
AuthorizationZonesManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
