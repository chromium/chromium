// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_IP_PROTECTION_CONFIG_PROVIDER_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_AW_IP_PROTECTION_CONFIG_PROVIDER_FACTORY_H_

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_ip_protection_config_provider.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace android_webview {

// Responsible for managing Android WebView IP Protection auth token fetching.
class AwIpProtectionConfigProviderFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AwIpProtectionConfigProvider* GetForAwBrowserContext(
      AwBrowserContext* aw_browser_context);

  static AwIpProtectionConfigProviderFactory* GetInstance();

  AwIpProtectionConfigProviderFactory(
      const AwIpProtectionConfigProviderFactory&) = delete;
  AwIpProtectionConfigProviderFactory& operator=(
      const AwIpProtectionConfigProviderFactory&) = delete;

 private:
  friend base::NoDestructor<AwIpProtectionConfigProviderFactory>;

  AwIpProtectionConfigProviderFactory();
  ~AwIpProtectionConfigProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_IP_PROTECTION_CONFIG_PROVIDER_FACTORY_H_
