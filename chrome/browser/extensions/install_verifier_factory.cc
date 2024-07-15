// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_verifier_factory.h"

#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"

using content::BrowserContext;

namespace extensions {

// static
InstallVerifier* InstallVerifierFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<InstallVerifier*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
InstallVerifierFactory* InstallVerifierFactory::GetInstance() {
  static base::NoDestructor<InstallVerifierFactory> instance;
  return instance.get();
}

InstallVerifierFactory::InstallVerifierFactory()
    : ProfileKeyedServiceFactory(
          "InstallVerifier",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

InstallVerifierFactory::~InstallVerifierFactory() = default;

std::unique_ptr<KeyedService>
InstallVerifierFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  return std::make_unique<InstallVerifier>(ExtensionPrefs::Get(context),
                                           context);
}

}  // namespace extensions
