// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_ALLOWLIST_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_ALLOWLIST_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ComponentExtensionContentSettingsAllowlist;

class ComponentExtensionContentSettingsAllowlistFactory final
    : public ProfileKeyedServiceFactory {
 public:
  static ComponentExtensionContentSettingsAllowlist* GetForBrowserContext(
      content::BrowserContext* context);
  static ComponentExtensionContentSettingsAllowlistFactory* GetInstance();

  ComponentExtensionContentSettingsAllowlistFactory(
      const ComponentExtensionContentSettingsAllowlistFactory&) = delete;
  ComponentExtensionContentSettingsAllowlistFactory& operator=(
      const ComponentExtensionContentSettingsAllowlistFactory&) = delete;

 private:
  friend base::NoDestructor<ComponentExtensionContentSettingsAllowlistFactory>;

  ComponentExtensionContentSettingsAllowlistFactory();
  ~ComponentExtensionContentSettingsAllowlistFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};
}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_ALLOWLIST_FACTORY_H_
