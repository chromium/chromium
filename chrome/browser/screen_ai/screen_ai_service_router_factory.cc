// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"

#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "content/public/browser/browser_context.h"

namespace screen_ai {

// static
screen_ai::ScreenAIServiceRouter*
ScreenAIServiceRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<screen_ai::ScreenAIServiceRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ScreenAIServiceRouterFactory* ScreenAIServiceRouterFactory::GetInstance() {
  static base::NoDestructor<ScreenAIServiceRouterFactory> instance;
  return instance.get();
}

ScreenAIServiceRouterFactory::ScreenAIServiceRouterFactory()
    : ProfileKeyedServiceFactory(
          "ScreenAIService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

ScreenAIServiceRouterFactory::~ScreenAIServiceRouterFactory() = default;

std::unique_ptr<KeyedService>
ScreenAIServiceRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* /*context*/) const {
  return base::WrapUnique<screen_ai::ScreenAIServiceRouter>(
      new screen_ai::ScreenAIServiceRouter);
}

// static
void ScreenAIServiceRouterFactory::EnsureFactoryBuilt() {
  ScreenAIServiceRouterFactory::GetInstance();
}

}  // namespace screen_ai
