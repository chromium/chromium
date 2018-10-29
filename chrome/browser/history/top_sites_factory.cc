// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_factory.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/engagement/top_sites/site_engagement_top_sites_provider.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/grit/theme_resources.h"
#include "components/history/core/browser/default_top_sites_provider.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "components/history/core/browser/top_sites_provider.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

const char kDisableTopSites[] = "disable-top-sites";

bool IsTopSitesDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableTopSites);
}

struct RawPrepopulatedPage {
  int url_id;            // The resource for the page URL.
  int title_id;          // The resource for the page title.
  int favicon_id;        // The raw data resource for the favicon.
  int large_favicon_id;  // The raw data resource for the larger favicon.
  int thumbnail_id;      // The raw data resource for the thumbnail.
  SkColor color;         // The best color to highlight the page (should
                         // roughly match favicon).
};

#if !defined(OS_ANDROID)
// Android does not use prepopulated pages.
const RawPrepopulatedPage kRawPrepopulatedPages[] = {
    {
        IDS_WEBSTORE_URL, IDS_EXTENSION_WEB_STORE_TITLE_SHORT,
        IDR_WEBSTORE_ICON_16, IDR_WEBSTORE_ICON_32,
        IDR_NEWTAB_WEBSTORE_THUMBNAIL, SkColorSetRGB(63, 132, 197),
    },
};
#endif

void InitializePrepopulatedPageList(
    PrefService* prefs,
    history::PrepopulatedPageList* prepopulated_pages) {
#if !defined(OS_ANDROID)
  DCHECK(prepopulated_pages);
  bool hide_web_store_icon = prefs->GetBoolean(prefs::kHideWebStoreIcon);
  prepopulated_pages->reserve(arraysize(kRawPrepopulatedPages));
  for (size_t i = 0; i < arraysize(kRawPrepopulatedPages); ++i) {
    const RawPrepopulatedPage& page = kRawPrepopulatedPages[i];
    if (hide_web_store_icon && page.url_id == IDS_WEBSTORE_URL)
      continue;
    prepopulated_pages->push_back(history::PrepopulatedPage(
        GURL(l10n_util::GetStringUTF8(page.url_id)),
        l10n_util::GetStringUTF16(page.title_id),
        features::IsMDIconsEnabled() ? page.large_favicon_id : page.favicon_id,
        page.thumbnail_id, page.color));
  }
#endif
}

std::unique_ptr<history::TopSitesProvider> CreateTopSitesProvider(
    Profile* profile,
    history::HistoryService* history_service) {
  if (base::FeatureList::IsEnabled(features::kTopSitesFromSiteEngagement)) {
    return std::make_unique<SiteEngagementTopSitesProvider>(
        SiteEngagementService::Get(profile), history_service);
  }

  return std::make_unique<history::DefaultTopSitesProvider>(history_service);
}

}  // namespace

// static
scoped_refptr<history::TopSites> TopSitesFactory::GetForProfile(
    Profile* profile) {
  if (IsTopSitesDisabled())
    return nullptr;
  return static_cast<history::TopSites*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
TopSitesFactory* TopSitesFactory::GetInstance() {
  return base::Singleton<TopSitesFactory>::get();
}

// static
scoped_refptr<history::TopSites> TopSitesFactory::BuildTopSites(
    content::BrowserContext* context,
    const std::vector<history::PrepopulatedPage>& prepopulated_page_list) {
  Profile* profile = Profile::FromBrowserContext(context);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  scoped_refptr<history::TopSitesImpl> top_sites(new history::TopSitesImpl(
      profile->GetPrefs(), history_service,
      CreateTopSitesProvider(profile, history_service), prepopulated_page_list,
      base::Bind(CanAddURLToHistory)));
  top_sites->Init(context->GetPath().Append(history::kTopSitesFilename));
  return top_sites;
}

TopSitesFactory::TopSitesFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "TopSites",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());

  // This dependency is only used when the experimental
  // kTopSitesFromSiteEngagement feature is active.
  DependsOn(SiteEngagementServiceFactory::GetInstance());
}

TopSitesFactory::~TopSitesFactory() {
}

scoped_refptr<RefcountedKeyedService> TopSitesFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  history::PrepopulatedPageList prepopulated_pages;
  InitializePrepopulatedPageList(
      Profile::FromBrowserContext(context)->GetPrefs(), &prepopulated_pages);
  return BuildTopSites(context, prepopulated_pages);
}

void TopSitesFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  history::TopSitesImpl::RegisterPrefs(registry);
}

bool TopSitesFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
