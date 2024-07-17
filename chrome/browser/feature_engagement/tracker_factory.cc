// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/tracker_factory.h"

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/feature_engagement/public/configuration_provider.h"
#include "components/feature_engagement/public/field_trial_configuration_provider.h"
#include "components/feature_engagement/public/local_configuration_provider.h"
#include "components/feature_engagement/public/tracker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/user_education/user_education_configuration_provider.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/growth/campaigns_configuration_provider.h"
#endif

namespace feature_engagement {

// static
TrackerFactory* TrackerFactory::GetInstance() {
  static base::NoDestructor<TrackerFactory> instance;
  return instance.get();
}

// static
feature_engagement::Tracker* TrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<feature_engagement::Tracker*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

TrackerFactory::TrackerFactory()
    : ProfileKeyedServiceFactory(
          "feature_engagement::Tracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // The service is needed by the System Profile OTR (that manages
              // the Profile Picker) to track elements for IPHs displayed in
              // the Profile Picker.
              .WithSystem(ProfileSelection::kOffTheRecordOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

TrackerFactory::~TrackerFactory() = default;

KeyedService* TrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  base::FilePath storage_dir = profile->GetPath().Append(
      chrome::kFeatureEngagementTrackerStorageDirname);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider();
  auto providers =
      feature_engagement::Tracker::GetDefaultConfigurationProviders();
#if !BUILDFLAG(IS_ANDROID)
  providers.emplace_back(
      std::make_unique<UserEducationConfigurationProvider>());
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  providers.emplace_back(
      std::make_unique<growth::CampaignsConfigurationProvider>());
#endif

  return feature_engagement::Tracker::Create(
      storage_dir, background_task_runner, db_provider, nullptr,
      std::move(providers));
}

}  // namespace feature_engagement
