// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREFS_FACTORY_H__
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREFS_FACTORY_H__

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {
namespace settings_private {

class GeneratedPrefs;

// BrowserContextKeyedServiceFactory for GeneratedPrefs.
class GeneratedPrefsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static GeneratedPrefs* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static GeneratedPrefsFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<GeneratedPrefsFactory>;

  GeneratedPrefsFactory();
  ~GeneratedPrefsFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(GeneratedPrefsFactory);
};

}  // namespace settings_private
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREFS_FACTORY_H__
