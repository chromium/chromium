// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_manager.h"

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {

// BrowserContextKeyedServiceFactory for ExtensionActionManager.
class ExtensionActionManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // BrowserContextKeyedServiceFactory implementation:
  static ExtensionActionManager* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<ExtensionActionManager*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static ExtensionActionManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ExtensionActionManagerFactory>;

  ExtensionActionManagerFactory()
      : BrowserContextKeyedServiceFactory(
          "ExtensionActionManager",
          BrowserContextDependencyManager::GetInstance()) {
  }

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override {
    return new ExtensionActionManager(static_cast<Profile*>(profile));
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
  }
};

ExtensionActionManagerFactory*
ExtensionActionManagerFactory::GetInstance() {
  return base::Singleton<ExtensionActionManagerFactory>::get();
}

}  // namespace

ExtensionActionManager::ExtensionActionManager(Profile* profile)
    : profile_(profile) {
  CHECK_EQ(profile, profile->GetOriginalProfile())
      << "Don't instantiate this with an incognito profile.";
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
}

ExtensionActionManager::~ExtensionActionManager() {
  // Don't assert that the ExtensionAction maps are empty because Extensions are
  // sometimes (only in tests?) not unloaded before the Profile is destroyed.
}

ExtensionActionManager* ExtensionActionManager::Get(
    content::BrowserContext* context) {
  return ExtensionActionManagerFactory::GetForBrowserContext(context);
}

void ExtensionActionManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  actions_.erase(extension->id());
}

ExtensionAction* ExtensionActionManager::GetExtensionAction(
    const Extension& extension) const {
  auto iter = actions_.find(extension.id());
  if (iter != actions_.end())
    return iter->second.get();

  const ActionInfo* action_info = ActionInfo::GetAnyActionInfo(&extension);
  if (!action_info)
    return nullptr;

  // Only create action info for enabled extensions.
  // This avoids bugs where actions are recreated just after being removed
  // in response to OnExtensionUnloaded().
  if (!ExtensionRegistry::Get(profile_)->enabled_extensions().Contains(
          extension.id())) {
    return nullptr;
  }

  auto action = std::make_unique<ExtensionAction>(extension, *action_info);

  if (action->default_icon()) {
    action->SetDefaultIconImage(std::make_unique<IconImage>(
        profile_, &extension, *action->default_icon(),
        ExtensionAction::ActionIconSize(),
        ExtensionAction::FallbackIcon().AsImageSkia(), nullptr));
  }

  ExtensionAction* raw_action = action.get();
  actions_[extension.id()] = std::move(action);
  return raw_action;
}

}  // namespace extensions
