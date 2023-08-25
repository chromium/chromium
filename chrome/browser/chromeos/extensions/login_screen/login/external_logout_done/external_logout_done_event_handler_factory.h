// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_DONE_EXTERNAL_LOGOUT_DONE_EVENT_HANDLER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_DONE_EXTERNAL_LOGOUT_DONE_EVENT_HANDLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class ExternalLogoutDoneEventHandler;

// Factory for the `ExternalLogoutDoneEventHandler` KeyedService.
class ExternalLogoutDoneEventHandlerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static ExternalLogoutDoneEventHandler* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static ExternalLogoutDoneEventHandlerFactory* GetInstance();

  ExternalLogoutDoneEventHandlerFactory(
      const ExternalLogoutDoneEventHandlerFactory&) = delete;
  ExternalLogoutDoneEventHandlerFactory& operator=(
      const ExternalLogoutDoneEventHandlerFactory&) = delete;

 private:
  friend class base::NoDestructor<ExternalLogoutDoneEventHandlerFactory>;

  ExternalLogoutDoneEventHandlerFactory();
  ~ExternalLogoutDoneEventHandlerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_DONE_EXTERNAL_LOGOUT_DONE_EVENT_HANDLER_FACTORY_H_
