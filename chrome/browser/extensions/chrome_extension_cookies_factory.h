// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class ChromeExtensionCookies;

class ChromeExtensionCookiesFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ChromeExtensionCookies* GetForBrowserContext(
      content::BrowserContext* context);
  static ChromeExtensionCookiesFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ChromeExtensionCookiesFactory>;

  ChromeExtensionCookiesFactory();
  ~ChromeExtensionCookiesFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionCookiesFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_FACTORY_H_
