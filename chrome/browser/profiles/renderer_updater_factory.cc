// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/renderer_updater_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/renderer_updater.h"
#include "chrome/browser/signin/identity_manager_factory.h"

RendererUpdaterFactory::RendererUpdaterFactory()
    : ProfileKeyedServiceFactory(
          "RendererUpdater",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

RendererUpdaterFactory::~RendererUpdaterFactory() {}

// static
RendererUpdaterFactory* RendererUpdaterFactory::GetInstance() {
  return base::Singleton<RendererUpdaterFactory>::get();
}

// static
RendererUpdater* RendererUpdaterFactory::GetForProfile(Profile* profile) {
  return static_cast<RendererUpdater*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* RendererUpdaterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new RendererUpdater(static_cast<Profile*>(context));
}

bool RendererUpdaterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
