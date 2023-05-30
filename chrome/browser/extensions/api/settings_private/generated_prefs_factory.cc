// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/generated_prefs_factory.h"

#include "chrome/browser/extensions/api/settings_private/generated_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"

namespace extensions {
namespace settings_private {

// static
GeneratedPrefs* GeneratedPrefsFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<GeneratedPrefs*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
GeneratedPrefsFactory* GeneratedPrefsFactory::GetInstance() {
  static base::NoDestructor<GeneratedPrefsFactory> instance;
  return instance.get();
}

GeneratedPrefsFactory::GeneratedPrefsFactory()
    : ProfileKeyedServiceFactory(
          "GeneratedPrefs",
          // Use |context| even if it is off-the-record/incognito.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

GeneratedPrefsFactory::~GeneratedPrefsFactory() = default;

bool GeneratedPrefsFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* GeneratedPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new GeneratedPrefs(static_cast<Profile*>(profile));
}

}  // namespace settings_private
}  // namespace extensions
