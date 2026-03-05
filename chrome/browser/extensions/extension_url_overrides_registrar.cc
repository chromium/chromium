// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_url_overrides_registrar.h"

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/one_shot_event.h"
#include "chrome/browser/extensions/extension_url_overrides.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ExtensionUrlOverridesRegistrar::ExtensionUrlOverridesRegistrar(
    content::BrowserContext* context) {
  ExtensionUrlOverrides::InitializeChromeURLOverrides(
      Profile::FromBrowserContext(context));
  extension_registry_observation_.Observe(ExtensionRegistry::Get(context));
  ExtensionSystem::Get(context)->ready().Post(
      FROM_HERE,
      base::BindOnce(&ExtensionUrlOverridesRegistrar::OnExtensionSystemReady,
                     weak_factory_.GetWeakPtr(), context));
}

ExtensionUrlOverridesRegistrar::~ExtensionUrlOverridesRegistrar() = default;

void ExtensionUrlOverridesRegistrar::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  const URLOverrides::URLOverrideMap& overrides =
      URLOverrides::GetChromeURLOverrides(extension);
  ExtensionUrlOverrides::RegisterOrActivateChromeURLOverrides(
      Profile::FromBrowserContext(browser_context), overrides);
  if (!overrides.empty()) {
    for (auto& observer : observer_list_) {
      observer.OnExtensionOverrideAdded(*extension);
    }
  }
}

void ExtensionUrlOverridesRegistrar::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  const URLOverrides::URLOverrideMap& overrides =
      URLOverrides::GetChromeURLOverrides(extension);
  ExtensionUrlOverrides::DeactivateChromeURLOverrides(
      Profile::FromBrowserContext(browser_context), overrides);
  if (!overrides.empty()) {
    for (auto& observer : observer_list_) {
      observer.OnExtensionOverrideRemoved(*extension);
    }
  }
}

void ExtensionUrlOverridesRegistrar::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  ExtensionUrlOverrides::UnregisterChromeURLOverrides(
      Profile::FromBrowserContext(browser_context),
      URLOverrides::GetChromeURLOverrides(extension));
}

void ExtensionUrlOverridesRegistrar::OnExtensionSystemReady(
    content::BrowserContext* context) {
  ExtensionUrlOverrides::ValidateChromeURLOverrides(
      Profile::FromBrowserContext(context));
}

void ExtensionUrlOverridesRegistrar::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionUrlOverridesRegistrar::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ExtensionUrlOverridesRegistrar>>::DestructorAtExit
    g_extension_web_ui_override_registrar_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ExtensionUrlOverridesRegistrar>*
ExtensionUrlOverridesRegistrar::GetFactoryInstance() {
  return g_extension_web_ui_override_registrar_factory.Pointer();
}

}  // namespace extensions
