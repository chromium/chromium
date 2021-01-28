// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/client_hints/client_hints_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/client_hints/browser/client_hints.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace {

base::LazyInstance<ClientHintsFactory>::DestructorAtExit
    g_client_hints_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
content::ClientHintsControllerDelegate*
ClientHintsFactory::GetForBrowserContext(content::BrowserContext* context) {
  return static_cast<client_hints::ClientHints*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ClientHintsFactory* ClientHintsFactory::GetInstance() {
  return g_client_hints_factory.Pointer();
}

ClientHintsFactory::ClientHintsFactory()
    : BrowserContextKeyedServiceFactory(
          "ClientHints",
          BrowserContextDependencyManager::GetInstance()) {}

ClientHintsFactory::~ClientHintsFactory() = default;

KeyedService* ClientHintsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  PrefService* local_state = g_browser_process->local_state();
  return new client_hints::ClientHints(
      context, g_browser_process->network_quality_tracker(),
      HostContentSettingsMapFactory::GetForProfile(context),
      embedder_support::GetUserAgentMetadata(), local_state);
}

content::BrowserContext* ClientHintsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool ClientHintsFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
