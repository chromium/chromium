// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/recovery/recovery_install_global_error_factory.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/recovery/recovery_install_global_error.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"

RecoveryInstallGlobalErrorFactory::RecoveryInstallGlobalErrorFactory()
    : ProfileKeyedServiceFactory(
          "RecoveryInstallGlobalError",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(GlobalErrorServiceFactory::GetInstance());
}

RecoveryInstallGlobalErrorFactory::~RecoveryInstallGlobalErrorFactory() =
    default;

// static
RecoveryInstallGlobalError*
RecoveryInstallGlobalErrorFactory::GetForProfile(Profile* profile) {
  return static_cast<RecoveryInstallGlobalError*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
RecoveryInstallGlobalErrorFactory*
RecoveryInstallGlobalErrorFactory::GetInstance() {
  static base::NoDestructor<RecoveryInstallGlobalErrorFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
RecoveryInstallGlobalErrorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return std::make_unique<RecoveryInstallGlobalError>(
      static_cast<Profile*>(context));
#else
  return NULL;
#endif
}
