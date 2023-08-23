// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class ChromeExtensionCookies;

class ChromeExtensionCookiesFactory : public ProfileKeyedServiceFactory {
 public:
  ChromeExtensionCookiesFactory(const ChromeExtensionCookiesFactory&) = delete;
  ChromeExtensionCookiesFactory& operator=(
      const ChromeExtensionCookiesFactory&) = delete;

  static ChromeExtensionCookies* GetForBrowserContext(
      content::BrowserContext* context);
  static ChromeExtensionCookiesFactory* GetInstance();

 private:
  friend base::NoDestructor<ChromeExtensionCookiesFactory>;

  ChromeExtensionCookiesFactory();
  ~ChromeExtensionCookiesFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_FACTORY_H_
