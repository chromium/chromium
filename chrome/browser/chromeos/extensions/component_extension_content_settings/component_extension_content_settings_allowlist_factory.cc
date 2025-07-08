// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_allowlist_factory.h"

#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_allowlist.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry_factory.h"

namespace extensions {

ComponentExtensionContentSettingsAllowlist*
ComponentExtensionContentSettingsAllowlistFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ComponentExtensionContentSettingsAllowlist*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ComponentExtensionContentSettingsAllowlistFactory*
ComponentExtensionContentSettingsAllowlistFactory::GetInstance() {
  static base::NoDestructor<ComponentExtensionContentSettingsAllowlistFactory>
      instance;
  return instance.get();
}

ComponentExtensionContentSettingsAllowlistFactory::
    ComponentExtensionContentSettingsAllowlistFactory()
    : ProfileKeyedServiceFactory(
          "ComponentExtensionContentSettingsAllowlist",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

ComponentExtensionContentSettingsAllowlistFactory::
    ~ComponentExtensionContentSettingsAllowlistFactory() = default;

std::unique_ptr<KeyedService>
ComponentExtensionContentSettingsAllowlistFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<ComponentExtensionContentSettingsAllowlist>(context);
}

}  // namespace extensions
