// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_cookies_factory.h"

#include "chrome/browser/extensions/chrome_extension_cookies.h"
#include "chrome/browser/profiles/profile.h"

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
    : ProfileKeyedServiceFactory(
          "ChromeExtensionCookies",
          // Incognito gets separate extension cookies, too.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

ChromeExtensionCookiesFactory::~ChromeExtensionCookiesFactory() {}

KeyedService* ChromeExtensionCookiesFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new ChromeExtensionCookies(static_cast<Profile*>(context));
}

}  // namespace extensions
