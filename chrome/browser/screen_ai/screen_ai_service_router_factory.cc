// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"

#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
    : BrowserContextKeyedServiceFactory(
          "ScreenAIService",
          BrowserContextDependencyManager::GetInstance()) {}

ScreenAIServiceRouterFactory::~ScreenAIServiceRouterFactory() = default;

KeyedService* ScreenAIServiceRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* /*context*/) const {
  return new screen_ai::ScreenAIServiceRouter();
}

// Incognito profiles should use their own instance.
content::BrowserContext* ScreenAIServiceRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

// static
void ScreenAIServiceRouterFactory::EnsureFactoryBuilt() {
  ScreenAIServiceRouterFactory::GetInstance();
}

}  // namespace screen_ai
