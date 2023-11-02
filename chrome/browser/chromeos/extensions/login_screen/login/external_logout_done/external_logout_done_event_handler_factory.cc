// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_done/external_logout_done_event_handler_factory.h"

#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_done/external_logout_done_event_handler.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"

namespace extensions {

// static
ExternalLogoutDoneEventHandler*
ExternalLogoutDoneEventHandlerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ExternalLogoutDoneEventHandler*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /* create= */ true));
}

// static
ExternalLogoutDoneEventHandlerFactory*
ExternalLogoutDoneEventHandlerFactory::GetInstance() {
  static base::NoDestructor<ExternalLogoutDoneEventHandlerFactory> instance;
  return instance.get();
}

ExternalLogoutDoneEventHandlerFactory::ExternalLogoutDoneEventHandlerFactory()
    : ProfileKeyedServiceFactory("ExternalLogoutDoneEventHandler") {
  DependsOn(EventRouterFactory::GetInstance());
}

ExternalLogoutDoneEventHandlerFactory::
    ~ExternalLogoutDoneEventHandlerFactory() = default;

KeyedService* ExternalLogoutDoneEventHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new ExternalLogoutDoneEventHandler(browser_context);
}

bool ExternalLogoutDoneEventHandlerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool ExternalLogoutDoneEventHandlerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace extensions
