// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/profile_metrics_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/profile_metrics_service.h"

// static
ProfileMetricsServiceFactory* ProfileMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<ProfileMetricsServiceFactory> factory;
  return factory.get();
}

// static
metrics::ProfileMetricsService* ProfileMetricsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<metrics::ProfileMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

ProfileMetricsServiceFactory::ProfileMetricsServiceFactory()
    : ProfileKeyedServiceFactory(
          "ProfileMetricsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithSystem(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

ProfileMetricsServiceFactory::~ProfileMetricsServiceFactory() = default;

std::unique_ptr<KeyedService>
ProfileMetricsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(metrics::features::kPerProfileMetrics)) {
    return std::make_unique<metrics::ProfileMetricsService>();
  }

  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);

  base::FilePath profile_path = profile->GetBaseName();
  CHECK(!profile_path.empty());

  if (profile_path == base::FilePath(chrome::kSystemProfileDir)) {
    // TODO(crbug.com/417921579): Decide how to log these profiles, for now just
    // don't log anything.
    return std::make_unique<metrics::ProfileMetricsService>();
  }

  if (profile_path.MaybeAsASCII() == chrome::kInitialProfile) {
    // Map the "Default" profile to "Profile0" in metrics.
    return std::make_unique<metrics::ProfileMetricsService>(0);
  }

  // We expect the profile path to be of the form "Profile N".
  std::optional<std::string_view> index_str = base::RemovePrefix(
      profile_path.MaybeAsASCII(), chrome::kMultiProfileDirPrefix);
  if (!index_str.has_value()) {
    // This may happen if the profile path has been manually overridden.
    return std::make_unique<metrics::ProfileMetricsService>();
  }

  size_t profile_index;
  if (!base::StringToSizeT(index_str.value(), &profile_index)) {
    // This may happen if the profile path has been manually overridden.
    return std::make_unique<metrics::ProfileMetricsService>();
  }

  return std::make_unique<metrics::ProfileMetricsService>(profile_index);
}
