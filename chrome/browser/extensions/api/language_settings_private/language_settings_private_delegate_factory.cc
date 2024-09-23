// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate_factory.h"

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
LanguageSettingsPrivateDelegate*
LanguageSettingsPrivateDelegateFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LanguageSettingsPrivateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
LanguageSettingsPrivateDelegateFactory*
LanguageSettingsPrivateDelegateFactory::GetInstance() {
  static base::NoDestructor<LanguageSettingsPrivateDelegateFactory> instance;
  return instance.get();
}

LanguageSettingsPrivateDelegateFactory::LanguageSettingsPrivateDelegateFactory()
    : ProfileKeyedServiceFactory(
          "LanguageSettingsPrivateDelegate",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(SpellcheckServiceFactory::GetInstance());
}

LanguageSettingsPrivateDelegateFactory::
    ~LanguageSettingsPrivateDelegateFactory() = default;

std::unique_ptr<KeyedService>
LanguageSettingsPrivateDelegateFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return LanguageSettingsPrivateDelegate::Create(context);
}

bool LanguageSettingsPrivateDelegateFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
