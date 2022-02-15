// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_ping_manager_factory.h"

#include "android_webview/browser/aw_browser_process.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/browser/ping_manager.h"

namespace safe_browsing {

// static
AwPingManagerFactory* AwPingManagerFactory::GetInstance() {
  static base::NoDestructor<AwPingManagerFactory> instance;
  return instance.get();
}

// static
PingManager* AwPingManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PingManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

AwPingManagerFactory::AwPingManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "AwSafeBrowsingPingManager",
          BrowserContextDependencyManager::GetInstance()) {}

AwPingManagerFactory::~AwPingManagerFactory() = default;

KeyedService* AwPingManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return PingManager::Create(safe_browsing::GetV4ProtocolConfig(
      GetProtocolConfigClientName(), /*disable_auto_update=*/false));
}

content::BrowserContext* AwPingManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

std::string AwPingManagerFactory::GetProtocolConfigClientName() const {
  // Return a webview specific client name, see crbug.com/732373 for details.
  return "android_webview";
}

}  // namespace safe_browsing
