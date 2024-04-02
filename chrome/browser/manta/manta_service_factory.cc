// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manta/manta_service_factory.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace manta {

// static
MantaService* MantaServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<MantaService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MantaServiceFactory* MantaServiceFactory::GetInstance() {
  static base::NoDestructor<MantaServiceFactory> instance;
  return instance.get();
}

MantaServiceFactory::MantaServiceFactory()
    : ProfileKeyedServiceFactory("MantaServiceFactory") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

MantaServiceFactory::~MantaServiceFactory() = default;

std::unique_ptr<KeyedService>
MantaServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* const context) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_demo_mode = ash::DemoSession::IsDeviceInDemoMode();
#else
  bool is_demo_mode = false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* const profile = Profile::FromBrowserContext(context);
  return std::make_unique<MantaService>(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      IdentityManagerFactory::GetForProfile(profile), is_demo_mode);
}

}  // namespace manta
