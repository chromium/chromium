// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/safe_browsing/chrome_v4_protocol_config_provider.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/browser/ping_manager.h"

namespace safe_browsing {

// static
ChromePingManagerFactory* ChromePingManagerFactory::GetInstance() {
  static base::NoDestructor<ChromePingManagerFactory> instance;
  return instance.get();
}

// static
PingManager* ChromePingManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PingManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

ChromePingManagerFactory::ChromePingManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeSafeBrowsingPingManager",
          BrowserContextDependencyManager::GetInstance()) {}

ChromePingManagerFactory::~ChromePingManagerFactory() = default;

KeyedService* ChromePingManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return PingManager::Create(
      GetV4ProtocolConfig(),
      g_browser_process->safe_browsing_service()->GetURLLoaderFactory(profile));
}

content::BrowserContext* ChromePingManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace safe_browsing
