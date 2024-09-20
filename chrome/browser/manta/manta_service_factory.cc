// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manta/manta_service_factory.h"

#include <memory>

#include "base/version.h"
#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chromeos/ash/components/channel/channel_info.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
    : ProfileKeyedServiceFactory(
          "MantaServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

MantaServiceFactory::~MantaServiceFactory() = default;

std::unique_ptr<KeyedService>
MantaServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* const context) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_demo_mode = ash::DemoSession::IsDeviceInDemoMode();
  version_info::Channel chrome_channel = ash::GetChannel();
#else
  bool is_demo_mode = false;
  version_info::Channel chrome_channel = version_info::Channel::UNKNOWN;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  Profile* const profile = Profile::FromBrowserContext(context);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  bool is_signed_in = identity_manager && identity_manager->HasPrimaryAccount(
                                              signin::ConsentLevel::kSync);
  bool is_otr_profile = !profile->IsRegularProfile() || !is_signed_in;

  std::string chrome_version = version_info::GetVersion().GetString();
  std::string locale;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (PrefService* pref_service = profile->GetPrefs()) {
    // Check to make sure that the locale pref is set before accessing.
    locale = pref_service->GetString(language::prefs::kApplicationLocale);
  }
#else
  locale = g_browser_process->GetApplicationLocale();
#endif

  return std::make_unique<MantaService>(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      identity_manager, is_demo_mode, is_otr_profile, chrome_version,
      chrome_channel, locale);
}

}  // namespace manta
