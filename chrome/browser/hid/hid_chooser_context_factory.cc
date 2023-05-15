// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_chooser_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/profiles/profile.h"

// static
HidChooserContextFactory* HidChooserContextFactory::GetInstance() {
  static base::NoDestructor<HidChooserContextFactory> factory;
  return factory.get();
}

// static
HidChooserContext* HidChooserContextFactory::GetForProfile(Profile* profile) {
  return static_cast<HidChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
HidChooserContext* HidChooserContextFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<HidChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

HidChooserContextFactory::HidChooserContextFactory()
    : ProfileKeyedServiceFactory(
          "HidChooserContext",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

HidChooserContextFactory::~HidChooserContextFactory() = default;

KeyedService* HidChooserContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new HidChooserContext(Profile::FromBrowserContext(context));
}

void HidChooserContextFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  auto* hid_chooser_context =
      GetForProfileIfExists(Profile::FromBrowserContext(context));
  if (hid_chooser_context)
    hid_chooser_context->FlushScheduledSaveSettingsCalls();
}
