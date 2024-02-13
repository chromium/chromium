// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

namespace ash::language_packs {

// Factory for `LanguagePackFontService`.
// This service is "internal" - other code should not access it, so
// `GetForProfile()` is omitted on purpose.
class LanguagePackFontServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static LanguagePackFontServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<LanguagePackFontServiceFactory>;

  LanguagePackFontServiceFactory();
  ~LanguagePackFontServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace ash::language_packs

#endif  // CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_FACTORY_H_
