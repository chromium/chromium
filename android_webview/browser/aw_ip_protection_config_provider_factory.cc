// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_ip_protection_config_provider_factory.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_ip_protection_config_provider.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace android_webview {

// static
AwIpProtectionConfigProvider*
AwIpProtectionConfigProviderFactory::GetForAwBrowserContext(
    AwBrowserContext* aw_browser_context) {
  return static_cast<AwIpProtectionConfigProvider*>(
      GetInstance()->GetServiceForBrowserContext(aw_browser_context,
                                                 /*create=*/true));
}

// static
AwIpProtectionConfigProviderFactory*
AwIpProtectionConfigProviderFactory::GetInstance() {
  static base::NoDestructor<AwIpProtectionConfigProviderFactory> instance;
  return instance.get();
}

AwIpProtectionConfigProviderFactory::AwIpProtectionConfigProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "AwIpProtectionConfigProviderFactory",
          BrowserContextDependencyManager::GetInstance()) {}

AwIpProtectionConfigProviderFactory::~AwIpProtectionConfigProviderFactory() =
    default;

content::BrowserContext*
AwIpProtectionConfigProviderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!AwIpProtectionConfigProvider::CanIpProtectionBeEnabled()) {
    return nullptr;
  }
  return BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context);
}

std::unique_ptr<KeyedService>
AwIpProtectionConfigProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  AwBrowserContext* aw_browser_context =
      static_cast<AwBrowserContext*>(context);
  return std::make_unique<AwIpProtectionConfigProvider>(aw_browser_context);
}
}  // namespace android_webview
