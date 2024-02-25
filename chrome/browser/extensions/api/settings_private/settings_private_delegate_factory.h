// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H__
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H__

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class SettingsPrivateDelegate;

// BrowserContextKeyedServiceFactory for each SettingsPrivateDelegate.
class SettingsPrivateDelegateFactory : public ProfileKeyedServiceFactory {
 public:
  SettingsPrivateDelegateFactory(const SettingsPrivateDelegateFactory&) =
      delete;
  SettingsPrivateDelegateFactory& operator=(
      const SettingsPrivateDelegateFactory&) = delete;

  static SettingsPrivateDelegate* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static SettingsPrivateDelegateFactory* GetInstance();

 private:
  friend base::NoDestructor<SettingsPrivateDelegateFactory>;

  SettingsPrivateDelegateFactory();
  ~SettingsPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_FACTORY_H__
