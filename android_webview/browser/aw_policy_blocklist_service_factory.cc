// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_policy_blocklist_service_factory.h"

#include <memory>

#include "android_webview/browser/aw_browser_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "components/policy/core/common/policy_pref_names.h"

namespace android_webview {

// static
PolicyBlocklistService* AwPolicyBlocklistServiceFactory::GetForBrowserContext(
    AwBrowserContext* context) {
  return static_cast<PolicyBlocklistService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AwPolicyBlocklistServiceFactory*
AwPolicyBlocklistServiceFactory::GetInstance() {
  static base::NoDestructor<AwPolicyBlocklistServiceFactory> instance;
  return instance.get();
}

AwPolicyBlocklistServiceFactory::AwPolicyBlocklistServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AwPolicyBlocklistService",
          BrowserContextDependencyManager::GetInstance()) {}

AwPolicyBlocklistServiceFactory::~AwPolicyBlocklistServiceFactory() = default;

std::unique_ptr<KeyedService>
AwPolicyBlocklistServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* aw_context = static_cast<AwBrowserContext*>(context);
  PrefService* pref_service = aw_context->GetPrefService();
  auto url_blocklist_manager = std::make_unique<policy::URLBlocklistManager>(
      pref_service, policy::policy_prefs::kUrlBlocklist,
      policy::policy_prefs::kUrlAllowlist);
  return std::make_unique<PolicyBlocklistService>(
      std::move(url_blocklist_manager), pref_service);
}

content::BrowserContext*
AwPolicyBlocklistServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace android_webview
