// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_IP_PROTECTION_AW_IP_PROTECTION_CORE_HOST_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_IP_PROTECTION_AW_IP_PROTECTION_CORE_HOST_FACTORY_H_

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/ip_protection/aw_ip_protection_core_host.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace android_webview {

// Responsible for managing Android WebView IP Protection auth token fetching.
class AwIpProtectionCoreHostFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AwIpProtectionCoreHost* GetForAwBrowserContext(
      AwBrowserContext* aw_browser_context);

  static AwIpProtectionCoreHostFactory* GetInstance();

  AwIpProtectionCoreHostFactory(
      const AwIpProtectionCoreHostFactory&) = delete;
  AwIpProtectionCoreHostFactory& operator=(
      const AwIpProtectionCoreHostFactory&) = delete;

 private:
  friend base::NoDestructor<AwIpProtectionCoreHostFactory>;

  AwIpProtectionCoreHostFactory();
  ~AwIpProtectionCoreHostFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_IP_PROTECTION_AW_IP_PROTECTION_CORE_HOST_FACTORY_H_
