// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/install_limiter_factory.h"

#include "chrome/browser/ash/extensions/install_limiter.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
InstallLimiter* InstallLimiterFactory::GetForProfile(Profile* profile) {
  return static_cast<InstallLimiter*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InstallLimiterFactory* InstallLimiterFactory::GetInstance() {
  static base::NoDestructor<InstallLimiterFactory> instance;
  return instance.get();
}

InstallLimiterFactory::InstallLimiterFactory()
    : ProfileKeyedServiceFactory(
          "InstallLimiter",
          ProfileSelections::Builder()
#if BUILDFLAG(IS_CHROMEOS_ASH)
              // Use OTR profile for Guest Session.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
#endif
              .Build()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

InstallLimiterFactory::~InstallLimiterFactory() = default;

KeyedService* InstallLimiterFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new InstallLimiter();
}

}  // namespace extensions
