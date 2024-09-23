// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/ip_protection/aw_ip_protection_core_host_factory.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/ip_protection/aw_ip_protection_core_host.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace android_webview {

// static
AwIpProtectionCoreHost*
AwIpProtectionCoreHostFactory::GetForAwBrowserContext(
    AwBrowserContext* aw_browser_context) {
  return static_cast<AwIpProtectionCoreHost*>(
      GetInstance()->GetServiceForBrowserContext(aw_browser_context,
                                                 /*create=*/true));
}

// static
AwIpProtectionCoreHostFactory*
AwIpProtectionCoreHostFactory::GetInstance() {
  static base::NoDestructor<AwIpProtectionCoreHostFactory> instance;
  return instance.get();
}

AwIpProtectionCoreHostFactory::AwIpProtectionCoreHostFactory()
    : BrowserContextKeyedServiceFactory(
          "AwIpProtectionCoreHostFactory",
          BrowserContextDependencyManager::GetInstance()) {}

AwIpProtectionCoreHostFactory::~AwIpProtectionCoreHostFactory() =
    default;

content::BrowserContext*
AwIpProtectionCoreHostFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!AwIpProtectionCoreHost::CanIpProtectionBeEnabled()) {
    return nullptr;
  }
  return BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context);
}

std::unique_ptr<KeyedService>
AwIpProtectionCoreHostFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  AwBrowserContext* aw_browser_context =
      static_cast<AwBrowserContext*>(context);
  return std::make_unique<AwIpProtectionCoreHost>(aw_browser_context);
}
}  // namespace android_webview
