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
  static base::NoDestructor<ChromeExtensionCookiesFactory> instance;
  return instance.get();
}

ChromeExtensionCookiesFactory::ChromeExtensionCookiesFactory()
    : ProfileKeyedServiceFactory(
          "ChromeExtensionCookies",
          // Incognito gets separate extension cookies, too.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

ChromeExtensionCookiesFactory::~ChromeExtensionCookiesFactory() = default;

std::unique_ptr<KeyedService>
ChromeExtensionCookiesFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  return std::make_unique<ChromeExtensionCookies>(
      static_cast<Profile*>(context));
}

}  // namespace extensions
