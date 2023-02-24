// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_request/external_logout_request_event_handler_factory.h"

#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_request/external_logout_request_event_handler.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"

namespace extensions {

// static
ExternalLogoutRequestEventHandler*
ExternalLogoutRequestEventHandlerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ExternalLogoutRequestEventHandler*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /* create= */ true));
}

// static
ExternalLogoutRequestEventHandlerFactory*
ExternalLogoutRequestEventHandlerFactory::GetInstance() {
  static base::NoDestructor<ExternalLogoutRequestEventHandlerFactory> instance;
  return instance.get();
}

ExternalLogoutRequestEventHandlerFactory::
    ExternalLogoutRequestEventHandlerFactory()
    : ProfileKeyedServiceFactory(
          "ExternalLogoutRequestEventHandler",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(EventRouterFactory::GetInstance());
}

ExternalLogoutRequestEventHandlerFactory::
    ~ExternalLogoutRequestEventHandlerFactory() = default;

KeyedService* ExternalLogoutRequestEventHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new ExternalLogoutRequestEventHandler(browser_context);
}

bool ExternalLogoutRequestEventHandlerFactory::ServiceIsNULLWhileTesting()
    const {
  return true;
}

bool ExternalLogoutRequestEventHandlerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
