// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/one_shot_event.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ExtensionWebUIOverrideRegistrar::ExtensionWebUIOverrideRegistrar(
    content::BrowserContext* context) {
  ExtensionWebUI::InitializeChromeURLOverrides(
      Profile::FromBrowserContext(context));
  extension_registry_observation_.Observe(ExtensionRegistry::Get(context));
  ExtensionSystem::Get(context)->ready().Post(
      FROM_HERE,
      base::BindOnce(&ExtensionWebUIOverrideRegistrar::OnExtensionSystemReady,
                     weak_factory_.GetWeakPtr(), context));
}

ExtensionWebUIOverrideRegistrar::~ExtensionWebUIOverrideRegistrar() = default;

void ExtensionWebUIOverrideRegistrar::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  const URLOverrides::URLOverrideMap& overrides =
      URLOverrides::GetChromeURLOverrides(extension);
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      Profile::FromBrowserContext(browser_context), overrides);
  if (!overrides.empty()) {
    for (auto& observer : observer_list_) {
      observer.OnExtensionOverrideAdded(*extension);
    }
  }
}

void ExtensionWebUIOverrideRegistrar::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  const URLOverrides::URLOverrideMap& overrides =
      URLOverrides::GetChromeURLOverrides(extension);
  ExtensionWebUI::DeactivateChromeURLOverrides(
      Profile::FromBrowserContext(browser_context), overrides);
  if (!overrides.empty()) {
    for (auto& observer : observer_list_) {
      observer.OnExtensionOverrideRemoved(*extension);
    }
  }
}

void ExtensionWebUIOverrideRegistrar::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  ExtensionWebUI::UnregisterChromeURLOverrides(
      Profile::FromBrowserContext(browser_context),
      URLOverrides::GetChromeURLOverrides(extension));
}

void ExtensionWebUIOverrideRegistrar::OnExtensionSystemReady(
    content::BrowserContext* context) {
  ExtensionWebUI::ValidateChromeURLOverrides(
      Profile::FromBrowserContext(context));
}

void ExtensionWebUIOverrideRegistrar::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionWebUIOverrideRegistrar::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ExtensionWebUIOverrideRegistrar>>::DestructorAtExit
    g_extension_web_ui_override_registrar_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ExtensionWebUIOverrideRegistrar>*
ExtensionWebUIOverrideRegistrar::GetFactoryInstance() {
  return g_extension_web_ui_override_registrar_factory.Pointer();
}

}  // namespace extensions
