// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_REQUEST_EXTERNAL_LOGOUT_REQUEST_EVENT_HANDLER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_REQUEST_EXTERNAL_LOGOUT_REQUEST_EVENT_HANDLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class ExternalLogoutRequestEventHandler;

// Factory for the `ExternalLogoutRequestEventHandler` KeyedService.
class ExternalLogoutRequestEventHandlerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static ExternalLogoutRequestEventHandler* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static ExternalLogoutRequestEventHandlerFactory* GetInstance();

  ExternalLogoutRequestEventHandlerFactory(
      const ExternalLogoutRequestEventHandlerFactory&) = delete;
  ExternalLogoutRequestEventHandlerFactory& operator=(
      const ExternalLogoutRequestEventHandlerFactory&) = delete;

 private:
  friend class base::NoDestructor<ExternalLogoutRequestEventHandlerFactory>;

  ExternalLogoutRequestEventHandlerFactory();
  ~ExternalLogoutRequestEventHandlerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_EXTERNAL_LOGOUT_REQUEST_EXTERNAL_LOGOUT_REQUEST_EVENT_HANDLER_FACTORY_H_
