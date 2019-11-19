// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H__
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H__

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace context {
class BrowserContext;
}

namespace extensions {
class SettingsPrivateDelegate;

// BrowserContextKeyedServiceFactory for each SettingsPrivateDelegate.
class SettingsPrivateDelegateFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static SettingsPrivateDelegate* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static SettingsPrivateDelegateFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<SettingsPrivateDelegateFactory>;

  SettingsPrivateDelegateFactory();
  ~SettingsPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
    content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateDelegateFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H__
