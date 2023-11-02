// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager_lacros_factory.h"

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager_lacros.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

// static
CleanupManagerLacros* CleanupManagerLacrosFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<CleanupManagerLacros*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /* create= */ true));
}

// static
CleanupManagerLacrosFactory* CleanupManagerLacrosFactory::GetInstance() {
  static base::NoDestructor<CleanupManagerLacrosFactory> instance;
  return instance.get();
}

CleanupManagerLacrosFactory::CleanupManagerLacrosFactory()
    : ProfileKeyedServiceFactory(
          "CleanupManagerLacros",
          // Service is available for incognito profiles.
          ProfileSelections::BuildForRegularAndIncognito()) {}

CleanupManagerLacrosFactory::~CleanupManagerLacrosFactory() = default;

KeyedService* CleanupManagerLacrosFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new CleanupManagerLacros(browser_context);
}

bool CleanupManagerLacrosFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool CleanupManagerLacrosFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace chromeos
