// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_tracker_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
InstallTracker* InstallTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<InstallTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

InstallTrackerFactory* InstallTrackerFactory::GetInstance() {
  static base::NoDestructor<InstallTrackerFactory> instance;
  return instance.get();
}

InstallTrackerFactory::InstallTrackerFactory()
    : ProfileKeyedServiceFactory(
          "InstallTracker",
          // The installs themselves are routed to the non-incognito profile and
          // so should the install progress.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(ExtensionPrefsFactory::GetInstance());
}

InstallTrackerFactory::~InstallTrackerFactory() = default;

std::unique_ptr<KeyedService>
InstallTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<InstallTracker>(context,
                                          ExtensionPrefs::Get(context));
}

}  // namespace extensions
