// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "content/public/browser/browser_thread.h"

// static
ProfileStatistics* ProfileStatisticsFactory::GetForProfile(Profile* profile) {
  return static_cast<ProfileStatistics*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileStatisticsFactory* ProfileStatisticsFactory::GetInstance() {
  return base::Singleton<ProfileStatisticsFactory>::get();
}

ProfileStatisticsFactory::ProfileStatisticsFactory()
    : ProfileKeyedServiceFactory("ProfileStatistics") {}

KeyedService* ProfileStatisticsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new ProfileStatistics(profile);
}
