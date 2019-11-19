// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_CHROME_COLORS_CHROME_COLORS_FACTORY_H_
#define CHROME_BROWSER_SEARCH_CHROME_COLORS_CHROME_COLORS_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chrome_colors {

class ChromeColorsService;

// Singleton that owns all ChromeColorsServices and associates them with
// Profiles.
class ChromeColorsFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the ChromeColorsService for |profile|.
  static ChromeColorsService* GetForProfile(Profile* profile);

  static ChromeColorsFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ChromeColorsFactory>;

  ChromeColorsFactory();
  ~ChromeColorsFactory() override;

  // Overrides from BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(ChromeColorsFactory);
};

}  // namespace chrome_colors

#endif  // CHROME_BROWSER_SEARCH_CHROME_COLORS_CHROME_COLORS_FACTORY_H_
