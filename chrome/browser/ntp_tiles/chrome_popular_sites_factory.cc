// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_tiles/chrome_popular_sites_factory.h"

#include "base/check_is_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#include "content/public/browser/storage_partition.h"

std::unique_ptr<ntp_tiles::PopularSites>
ChromePopularSitesFactory::NewForProfile(Profile* profile) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  // PopularSitesImpl requires a TemplateURLService to resolve the country
  // code (see PopularSitesImpl::GetCountryToFetch). TemplateURLServiceFactory
  // is null-while-testing, so TestingProfile-based fixtures would otherwise
  // build an unusable instance. This factory is only reached on Android,
  // where every real profile has a TemplateURLService, so this path must not
  // be taken in production.
  if (!template_url_service) {
    CHECK_IS_TEST(base::NotFatalUntil::M158);
    return nullptr;
  }

  return std::make_unique<ntp_tiles::PopularSitesImpl>(
      profile->GetPrefs(), template_url_service,
      g_browser_process->variations_service(),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}
