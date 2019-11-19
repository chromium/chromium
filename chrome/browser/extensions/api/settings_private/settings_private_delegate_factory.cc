// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_delegate_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
  return base::Singleton<SettingsPrivateDelegateFactory>::get();
}

SettingsPrivateDelegateFactory::SettingsPrivateDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "SettingsPrivateDelegate",
          BrowserContextDependencyManager::GetInstance()) {
}

SettingsPrivateDelegateFactory::~SettingsPrivateDelegateFactory() {
}

content::BrowserContext* SettingsPrivateDelegateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use the incognito profile when in Guest mode.
  return context;
}

KeyedService* SettingsPrivateDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SettingsPrivateDelegate(static_cast<Profile*>(profile));
}

}  // namespace extensions
