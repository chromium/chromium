// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class LanguageSettingsPrivateDelegate;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the languageSettingsPrivate delegate per browsing context
// (since the extension event router and language preferences are per browsing
// context).
class LanguageSettingsPrivateDelegateFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the LanguageSettingsPrivateDelegate for |context|, creating it
  // if it is not yet created.
  static LanguageSettingsPrivateDelegate* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns the LanguageSettingsPrivateDelegateFactory instance.
  static LanguageSettingsPrivateDelegateFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend struct base::DefaultSingletonTraits<
      LanguageSettingsPrivateDelegateFactory>;

  LanguageSettingsPrivateDelegateFactory();
  ~LanguageSettingsPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateDelegateFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H_
