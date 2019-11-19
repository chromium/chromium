// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_cookies_factory.h"

#include "chrome/browser/extensions/chrome_extension_cookies.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

using content::BrowserContext;

namespace extensions {

// static
ChromeExtensionCookies* ChromeExtensionCookiesFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ChromeExtensionCookies*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ChromeExtensionCookiesFactory* ChromeExtensionCookiesFactory::GetInstance() {
  return base::Singleton<ChromeExtensionCookiesFactory>::get();
}

ChromeExtensionCookiesFactory::ChromeExtensionCookiesFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeExtensionCookies",
          BrowserContextDependencyManager::GetInstance()) {}

ChromeExtensionCookiesFactory::~ChromeExtensionCookiesFactory() {}

KeyedService* ChromeExtensionCookiesFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new ChromeExtensionCookies(static_cast<Profile*>(context));
}

BrowserContext* ChromeExtensionCookiesFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Incognito gets separate extension cookies, too.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace extensions
