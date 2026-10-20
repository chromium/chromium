// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_factory.h"

#include <stddef.h>

#include <array>
#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "base/feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/search/search.h"
#endif

namespace {

const char kDisableTopSites[] = "disable-top-sites";

bool IsTopSitesDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableTopSites);
}

struct RawPrepopulatedPage {
  int url_id;            // The resource for the page URL.
  int title_id;          // The resource for the page title.
  int favicon_id;        // The raw data resource for the favicon.
  SkColor color;         // The best color to highlight the page (should
                         // roughly match favicon).
};

// Prepopulated pages are a Desktop concept. On Android they are only available
// where the extensions platform is built in, i.e. on Desktop Android.
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS_CORE)
constexpr auto kRawPrepopulatedPages = std::to_array<RawPrepopulatedPage>({
    {
        IDS_WEBSTORE_URL,
        IDS_EXTENSION_WEB_STORE_TITLE_SHORT,
        IDR_WEBSTORE_ICON_32,
        SkColorSetRGB(63, 132, 197),
    },
});

bool ShouldUsePrepopulatedPages(Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/573330311): Add prepopulated pages for 1P WebUI NTPs.
  // On Desktop Android, prepopulated pages are only used by the WebUI NTP for
  // 3P DSEs, which mirrors the Desktop WebUI NTP behavior. The 1P WebUI NTP
  // will need to eventually show prepopulated pages to achieve feature parity
  // with Desktop.
  if (search::IsWebUiNtpEnabledForDesktopAndroid() &&
      base::FeatureList::IsEnabled(chrome::android::kUseWebUiNtp3PDSE)) {
    // NOTE: TopSites is created once per profile, so this decision reflects
    // the DSE at startup. If the user switches between a 1P and a 3P DSE at
    // runtime, the prepopulated pages stay stale until the next restart: 1P
    // DSE users keep seeing them and 3P DSE users do not get them yet. DSE
    // changes are rare, and once crbug.com/573330311 is fixed the decision no
    // longer depends on the DSE, so this goes away.
    return !search::DefaultSearchProviderIsGoogle(profile);
  }
  return false;
#else
  return true;
#endif  // BUILDFLAG(IS_ANDROID)
}
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS_CORE)

void InitializePrepopulatedPageList(
    Profile* profile,
    history::PrepopulatedPageList* prepopulated_pages) {
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (!ShouldUsePrepopulatedPages(profile)) {
    return;
  }

  CHECK(prepopulated_pages, base::NotFatalUntil::M161);
  PrefService* pref_service = profile->GetPrefs();
  bool hide_web_store_icon =
      pref_service->GetBoolean(policy::policy_prefs::kHideWebStoreIcon);

  prepopulated_pages->reserve(std::size(kRawPrepopulatedPages));
  for (size_t i = 0; i < std::size(kRawPrepopulatedPages); ++i) {
    const RawPrepopulatedPage& page = kRawPrepopulatedPages[i];
    if (hide_web_store_icon && page.url_id == IDS_WEBSTORE_URL)
      continue;

    prepopulated_pages->push_back(history::PrepopulatedPage(
        GURL(l10n_util::GetStringUTF8(page.url_id)),
        l10n_util::GetStringUTF16(page.title_id), page.favicon_id, page.color));
  }
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS_CORE)
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
  static base::NoDestructor<TopSitesFactory> instance;
  return instance.get();
}

// static
scoped_refptr<history::TopSites> TopSitesFactory::BuildTopSites(
    content::BrowserContext* context,
    const std::vector<history::PrepopulatedPage>& prepopulated_page_list) {
  Profile* profile = Profile::FromBrowserContext(context);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  scoped_refptr<history::TopSitesImpl> top_sites(new history::TopSitesImpl(
      profile->GetPrefs(), history_service, template_url_service,
      prepopulated_page_list, base::BindRepeating(CanAddURLToHistory)));
  top_sites->Init(context->GetPath().Append(history::kTopSitesFilename));
  return top_sites;
}

// static
RefcountedBrowserContextKeyedServiceFactory::TestingFactory
TopSitesFactory::GetDefaultFactory() {
  return base::BindRepeating([](content::BrowserContext* context)
                                 -> scoped_refptr<RefcountedKeyedService> {
    return GetInstance()->BuildServiceInstanceFor(context);
  });
}

TopSitesFactory::TopSitesFactory()
    : RefcountedProfileKeyedServiceFactory(
          "TopSites",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  // This dependency is only used when the experimental
  // kTopSitesFromSiteEngagement feature is active.
  DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
}

TopSitesFactory::~TopSitesFactory() = default;

scoped_refptr<RefcountedKeyedService> TopSitesFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  history::PrepopulatedPageList prepopulated_pages;
  InitializePrepopulatedPageList(Profile::FromBrowserContext(context),
                                 &prepopulated_pages);
  return BuildTopSites(context, prepopulated_pages);
}

void TopSitesFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  history::TopSitesImpl::RegisterPrefs(registry);
}

bool TopSitesFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
