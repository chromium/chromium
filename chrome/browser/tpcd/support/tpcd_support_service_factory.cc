// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/support/tpcd_support_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/origin_trials/origin_trials_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/support/tpcd_support_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "net/base/features.h"

namespace tpcd::trial {

// static
TpcdTrialServiceFactory* TpcdTrialServiceFactory::GetInstance() {
  static base::NoDestructor<TpcdTrialServiceFactory> factory;
  return factory.get();
}

// static
TpcdTrialService* TpcdTrialServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TpcdTrialService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileSelections TpcdTrialServiceFactory::CreateProfileSelections() {
  if (!base::FeatureList::IsEnabled(net::features::kTpcdTrialSettings) ||
      !base::FeatureList::IsEnabled(features::kPersistentOriginTrials)) {
    return ProfileSelections::BuildNoProfilesSelected();
  }

  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOriginalOnly)
      .WithGuest(ProfileSelection::kOwnInstance)
      // The Following will be completely unselected as users do not "browse"
      // within these profiles.
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

TpcdTrialServiceFactory::TpcdTrialServiceFactory()
    : ProfileKeyedServiceFactory("TpcdTrialService",
                                 CreateProfileSelections()) {
  DependsOn(OriginTrialsFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

TpcdTrialServiceFactory::~TpcdTrialServiceFactory() = default;

KeyedService* TpcdTrialServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return new TpcdTrialService(context);
}

}  // namespace tpcd::trial
