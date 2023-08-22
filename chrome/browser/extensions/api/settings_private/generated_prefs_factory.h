// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREFS_FACTORY_H__
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREFS_FACTORY_H__

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {
namespace settings_private {

class GeneratedPrefs;

// BrowserContextKeyedServiceFactory for GeneratedPrefs.
class GeneratedPrefsFactory : public ProfileKeyedServiceFactory {
 public:
  GeneratedPrefsFactory(const GeneratedPrefsFactory&) = delete;
  GeneratedPrefsFactory& operator=(const GeneratedPrefsFactory&) = delete;

  static GeneratedPrefs* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static GeneratedPrefsFactory* GetInstance();

 private:
  friend base::NoDestructor<GeneratedPrefsFactory>;

  GeneratedPrefsFactory();
  ~GeneratedPrefsFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  bool ServiceIsNULLWhileTesting() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace settings_private
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREFS_FACTORY_H__
