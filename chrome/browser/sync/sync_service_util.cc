// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_service_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/sync/base/features.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#include "components/variations/service/variations_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

bool IsDesktopEnUSLocaleOnlySyncPollFeatureEnabled() {
  if (base::FeatureList::GetInstance()->IsFeatureOverridden(
          syncer::kSyncPollImmediatelyOnEveryStartup.name)) {
    return base::FeatureList::IsEnabled(
        syncer::kSyncPollImmediatelyOnEveryStartup);
  }

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  std::string country_code;
  auto* variations_service = g_browser_process->variations_service();
  if (variations_service) {
    country_code = variations_service->GetStoredPermanentCountry();
    if (country_code.empty()) {
      country_code = variations_service->GetLatestCountry();
    }
  }

  return base::FeatureList::IsEnabled(
             syncer::kSyncPollImmediatelyOnEveryStartup) &&
         g_browser_process->GetApplicationLocale() == "en-US" &&
         country_code == "us";
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
}
