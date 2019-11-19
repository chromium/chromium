// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_metrics_service.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "content/public/browser/browser_url_handler.h"
#include "url/gurl.h"

namespace {

#if !defined(OS_ANDROID)
// Record a sample for the Settings.NewTabPage rappor metric.
void SampleNewTabPageURL(Profile* profile) {
  GURL ntp_url(chrome::kChromeUINewTabURL);
  bool reverse_on_redirect = false;
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &ntp_url,
      profile,
      &reverse_on_redirect);
  if (ntp_url.is_valid()) {
    rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                            "Settings.NewTabPage", ntp_url);
  }
}
#endif

}  // namespace

PrefMetricsService::PrefMetricsService(Profile* profile)
    : profile_(profile), prefs_(profile_->GetPrefs()) {
  RecordLaunchPrefs();
}

PrefMetricsService::~PrefMetricsService() {
}

// static
void PrefMetricsService::RecordHomePageLaunchMetrics(bool show_home_button,
                                                     bool homepage_is_ntp,
                                                     const GURL& homepage_url) {
  UMA_HISTOGRAM_BOOLEAN("Settings.ShowHomeButton", show_home_button);
  if (show_home_button) {
    UMA_HISTOGRAM_BOOLEAN("Settings.GivenShowHomeButton_HomePageIsNewTabPage",
                          homepage_is_ntp);
  }

  // For non-NTP homepages, see if the URL comes from the same TLD+1 as a known
  // search engine.  Note that this is only an approximation of search engine
  // use, due to both false negatives (pages that come from unknown TLD+1 X but
  // consist of a search box that sends to known TLD+1 Y) and false positives
  // (pages that share a TLD+1 with a known engine but aren't actually search
  // pages, e.g. plus.google.com).  Additionally, record the TLD+1 of non-NTP
  // homepages through the privacy-preserving Rappor service.
  if (!homepage_is_ntp) {
    if (homepage_url.is_valid()) {
      UMA_HISTOGRAM_ENUMERATION(
          "Settings.HomePageEngineType",
          TemplateURLPrepopulateData::GetEngineType(homepage_url),
          SEARCH_ENGINE_MAX);
      rappor::SampleDomainAndRegistryFromGURL(
          g_browser_process->rappor_service(), "Settings.HomePage2",
          homepage_url);
    }
  }
}

void PrefMetricsService::RecordLaunchPrefs() {
  // On Android, determining whether the homepage is enabled requires waiting
  // for a response from a third party provider installed on the device.  So,
  // it will be logged later once all the dependent information is available.
  // See DeferredStartupHandler.java.
#if !defined(OS_ANDROID)
  GURL homepage_url(prefs_->GetString(prefs::kHomePage));
  RecordHomePageLaunchMetrics(prefs_->GetBoolean(prefs::kShowHomeButton),
                              prefs_->GetBoolean(prefs::kHomePageIsNewTabPage),
                              homepage_url);
#endif

  // Android does not support overriding the NTP URL.
#if !defined(OS_ANDROID)
  SampleNewTabPageURL(profile_);
#endif

  // Tab restoring is always done on Android, so these metrics are not
  // applicable.  Also, startup pages are not supported on Android
#if !defined(OS_ANDROID)
  int restore_on_startup = prefs_->GetInteger(prefs::kRestoreOnStartup);
  UMA_HISTOGRAM_ENUMERATION(
      "Settings.StartupPageLoadSettings", restore_on_startup,
      static_cast<int>(SessionStartupPref::kPrefValueMax));
  if (restore_on_startup == SessionStartupPref::kPrefValueURLs) {
    const base::ListValue* url_list =
        prefs_->GetList(prefs::kURLsToRestoreOnStartup);
    // Similarly, check startup pages for known search engine TLD+1s.
    std::string url_text;
    for (size_t i = 0; i < url_list->GetSize(); ++i) {
      if (url_list->GetString(i, &url_text)) {
        GURL start_url(url_text);
        if (start_url.is_valid()) {
          UMA_HISTOGRAM_ENUMERATION(
              "Settings.StartupPageEngineTypes",
              TemplateURLPrepopulateData::GetEngineType(start_url),
              SEARCH_ENGINE_MAX);
          if (i == 0) {
            rappor::SampleDomainAndRegistryFromGURL(
                g_browser_process->rappor_service(),
                "Settings.FirstStartupPage", start_url);
          }
        }
      }
    }
  }
#endif

  // Android does not support pinned tabs.
#if !defined(OS_ANDROID)
  StartupTabs startup_tabs = PinnedTabCodec::ReadPinnedTabs(profile_);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Settings.PinnedTabs",
                              startup_tabs.size(), 1, 50, 20);
  for (size_t i = 0; i < startup_tabs.size(); ++i) {
    GURL start_url(startup_tabs.at(i).url);
    if (start_url.is_valid()) {
      UMA_HISTOGRAM_ENUMERATION(
          "Settings.PinnedTabEngineTypes",
          TemplateURLPrepopulateData::GetEngineType(start_url),
          SEARCH_ENGINE_MAX);
    }
  }
#endif
}

// static
PrefMetricsService::Factory* PrefMetricsService::Factory::GetInstance() {
  return base::Singleton<PrefMetricsService::Factory>::get();
}

// static
PrefMetricsService* PrefMetricsService::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<PrefMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrefMetricsService::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
        "PrefMetricsService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

PrefMetricsService::Factory::~Factory() {
}

KeyedService* PrefMetricsService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new PrefMetricsService(static_cast<Profile*>(profile));
}

bool PrefMetricsService::Factory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool PrefMetricsService::Factory::ServiceIsNULLWhileTesting() const {
  return false;
}

content::BrowserContext* PrefMetricsService::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
