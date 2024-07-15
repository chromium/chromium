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
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

HidChooserContextFactory::~HidChooserContextFactory() = default;

std::unique_ptr<KeyedService>
HidChooserContextFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<HidChooserContext>(
      Profile::FromBrowserContext(context));
}
