// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_delegate_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system_provider.h"

namespace extensions {

using content::BrowserContext;

// static
SettingsPrivateDelegate* SettingsPrivateDelegateFactory::GetForBrowserContext(
    BrowserContext* browser_context) {
  return static_cast<SettingsPrivateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
SettingsPrivateDelegateFactory* SettingsPrivateDelegateFactory::GetInstance() {
  static base::NoDestructor<SettingsPrivateDelegateFactory> instance;
  return instance.get();
}

SettingsPrivateDelegateFactory::SettingsPrivateDelegateFactory()
    : ProfileKeyedServiceFactory(
          "SettingsPrivateDelegate",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

SettingsPrivateDelegateFactory::~SettingsPrivateDelegateFactory() = default;

std::unique_ptr<KeyedService>
SettingsPrivateDelegateFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<SettingsPrivateDelegate>(
      static_cast<Profile*>(profile));
}

}  // namespace extensions
