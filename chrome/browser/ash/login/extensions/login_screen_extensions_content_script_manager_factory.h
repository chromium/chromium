// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_CONTENT_SCRIPT_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_CONTENT_SCRIPT_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace ash {

class LoginScreenExtensionsContentScriptManager;

// Factory for the `LoginScreenExtensionsContentScriptManager` KeyedService.
class LoginScreenExtensionsContentScriptManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static LoginScreenExtensionsContentScriptManager* GetForProfile(
      Profile* profile);
  static LoginScreenExtensionsContentScriptManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<
      LoginScreenExtensionsContentScriptManagerFactory>;

  LoginScreenExtensionsContentScriptManagerFactory();
  ~LoginScreenExtensionsContentScriptManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_CONTENT_SCRIPT_MANAGER_FACTORY_H_
