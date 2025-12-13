// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_POLICY_BLOCKLIST_SERVICE_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_AW_POLICY_BLOCKLIST_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class PolicyBlocklistService;

namespace android_webview {

class AwBrowserContext;

// Factory for PolicyBlocklistService in android webview.
class AwPolicyBlocklistServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AwPolicyBlocklistServiceFactory* GetInstance();

  static PolicyBlocklistService* GetForBrowserContext(
      AwBrowserContext* context);

  AwPolicyBlocklistServiceFactory(const AwPolicyBlocklistServiceFactory&) =
      delete;
  AwPolicyBlocklistServiceFactory& operator=(
      const AwPolicyBlocklistServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AwPolicyBlocklistServiceFactory>;

  AwPolicyBlocklistServiceFactory();
  ~AwPolicyBlocklistServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // Finds which browser context (if any) to use.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_POLICY_BLOCKLIST_SERVICE_FACTORY_H_
