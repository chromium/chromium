// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/service/accessibility_service_router_factory.h"
#include "chrome/browser/accessibility/service/accessibility_service_router.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"

namespace ax {

// static
AccessibilityServiceRouter*
AccessibilityServiceRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AccessibilityServiceRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AccessibilityServiceRouterFactory*
AccessibilityServiceRouterFactory::GetInstance() {
  static base::NoDestructor<AccessibilityServiceRouterFactory> instance;
  return instance.get();
}

AccessibilityServiceRouterFactory::AccessibilityServiceRouterFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityService",
          ProfileSelections::Builder()
              // TODO(b/262637071): Ensure A11yService can run on all profiles,
              // there should be no UI surface that doesn't allow the
              // A11yService, as it is required to run A11y features like
              // ChromeVox.
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithSystem(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

AccessibilityServiceRouterFactory::~AccessibilityServiceRouterFactory() =
    default;

std::unique_ptr<KeyedService>
AccessibilityServiceRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AccessibilityServiceRouter>();
}

// static
void AccessibilityServiceRouterFactory::EnsureFactoryBuilt() {
  GetInstance();
}

}  // namespace ax
