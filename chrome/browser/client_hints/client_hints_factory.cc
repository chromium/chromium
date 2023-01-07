// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/client_hints/client_hints_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/client_hints/browser/client_hints.h"
#include "components/embedder_support/user_agent_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

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
    : ProfileKeyedServiceFactory(
          "ClientHints",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(CookieSettingsFactory::GetInstance());
}

ClientHintsFactory::~ClientHintsFactory() = default;

KeyedService* ClientHintsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // CookieSettingsFactory::GetForProfile can only be called on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return new client_hints::ClientHints(
      context, g_browser_process->network_quality_tracker(),
      HostContentSettingsMapFactory::GetForProfile(context),
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(context)),
      g_browser_process->local_state());
}

bool ClientHintsFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
