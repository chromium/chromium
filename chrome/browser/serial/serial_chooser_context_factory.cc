// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_context.h"

SerialChooserContextFactory::SerialChooserContextFactory()
    : ProfileKeyedServiceFactory(
          "SerialChooserContext",
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

SerialChooserContextFactory::~SerialChooserContextFactory() = default;

std::unique_ptr<KeyedService>
SerialChooserContextFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SerialChooserContext>(
      Profile::FromBrowserContext(context));
}

// static
SerialChooserContextFactory* SerialChooserContextFactory::GetInstance() {
  static base::NoDestructor<SerialChooserContextFactory> instance;
  return instance.get();
}

// static
SerialChooserContext* SerialChooserContextFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SerialChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SerialChooserContext* SerialChooserContextFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<SerialChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}
