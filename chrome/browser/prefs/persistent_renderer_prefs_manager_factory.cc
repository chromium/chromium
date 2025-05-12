// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/persistent_renderer_prefs_manager_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/prefs/persistent_renderer_prefs_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

// static
PersistentRendererPrefsManager*
PersistentRendererPrefsManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<PersistentRendererPrefsManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PersistentRendererPrefsManagerFactory*
PersistentRendererPrefsManagerFactory::GetInstance() {
  static base::NoDestructor<PersistentRendererPrefsManagerFactory> instance;
  return instance.get();
}

PersistentRendererPrefsManagerFactory::PersistentRendererPrefsManagerFactory()
    : ProfileKeyedServiceFactory(
          "PersistentRendererPrefsManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

PersistentRendererPrefsManagerFactory::
    ~PersistentRendererPrefsManagerFactory() = default;

std::unique_ptr<KeyedService>
PersistentRendererPrefsManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<PersistentRendererPrefsManager>(
      *Profile::FromBrowserContext(context)->GetPrefs());
}
