// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"

// static
BluetoothChooserContextFactory* BluetoothChooserContextFactory::GetInstance() {
  static base::NoDestructor<BluetoothChooserContextFactory> factory;
  return factory.get();
}

// static
permissions::BluetoothChooserContext*
BluetoothChooserContextFactory::GetForProfile(Profile* profile) {
  return static_cast<permissions::BluetoothChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
permissions::BluetoothChooserContext*
BluetoothChooserContextFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<permissions::BluetoothChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

BluetoothChooserContextFactory::BluetoothChooserContextFactory()
    : ProfileKeyedServiceFactory(
          "BluetoothChooserContext",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

BluetoothChooserContextFactory::~BluetoothChooserContextFactory() = default;

KeyedService* BluetoothChooserContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::BluetoothChooserContext(context);
}

void BluetoothChooserContextFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  auto* bluetooth_chooser_context =
      GetForProfileIfExists(Profile::FromBrowserContext(context));
  if (bluetooth_chooser_context)
    bluetooth_chooser_context->FlushScheduledSaveSettingsCalls();
}
