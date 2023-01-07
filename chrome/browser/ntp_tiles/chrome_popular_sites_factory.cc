// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_tiles/chrome_popular_sites_factory.h"

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

std::unique_ptr<ntp_tiles::PopularSites>
ChromePopularSitesFactory::NewForProfile(Profile* profile) {
  return std::make_unique<ntp_tiles::PopularSitesImpl>(
      profile->GetPrefs(), TemplateURLServiceFactory::GetForProfile(profile),
      g_browser_process->variations_service(),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}
