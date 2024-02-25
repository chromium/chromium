// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class LanguageSettingsPrivateDelegate;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the languageSettingsPrivate delegate per browsing context
// (since the extension event router and language preferences are per browsing
// context).
class LanguageSettingsPrivateDelegateFactory
    : public ProfileKeyedServiceFactory {
 public:
  LanguageSettingsPrivateDelegateFactory(
      const LanguageSettingsPrivateDelegateFactory&) = delete;
  LanguageSettingsPrivateDelegateFactory& operator=(
      const LanguageSettingsPrivateDelegateFactory&) = delete;

  // Returns the LanguageSettingsPrivateDelegate for |context|, creating it
  // if it is not yet created.
  static LanguageSettingsPrivateDelegate* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns the LanguageSettingsPrivateDelegateFactory instance.
  static LanguageSettingsPrivateDelegateFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<LanguageSettingsPrivateDelegateFactory>;

  LanguageSettingsPrivateDelegateFactory();
  ~LanguageSettingsPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H_
