// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_metrics_service.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_utils.h"
#include "url/gurl.h"

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
  UMA_HISTOGRAM_BOOLEAN("Settings.ShowHomeButton2", show_home_button);
  if (show_home_button) {
    UMA_HISTOGRAM_BOOLEAN("Settings.GivenShowHomeButton_HomePageIsNewTabPage2",
                          homepage_is_ntp);
  }

  // For non-NTP homepages, see if the URL comes from the same TLD+1 as a known
  // search engine.  Note that this is only an approximation of search engine
  // use, due to both false negatives (pages that come from unknown TLD+1 X but
  // consist of a search box that sends to known TLD+1 Y) and false positives
  // (pages that share a TLD+1 with a known engine but aren't actually search
  // pages, e.g. plus.google.com).
  if (!homepage_is_ntp) {
    if (homepage_url.is_valid()) {
      UMA_HISTOGRAM_ENUMERATION("Settings.HomePageEngineType2",
                                SearchEngineUtils::GetEngineType(homepage_url),
                                SEARCH_ENGINE_MAX);
    }
  }
}

void PrefMetricsService::RecordLaunchPrefs() {
  // On Android, determining whether the homepage is enabled requires waiting
  // for a response from a third party provider installed on the device.  So,
  // it will be logged later once all the dependent information is available.
  // See DeferredStartupHandler.java.
#if !BUILDFLAG(IS_ANDROID)
  GURL homepage_url(prefs_->GetString(prefs::kHomePage));
  RecordHomePageLaunchMetrics(prefs_->GetBoolean(prefs::kShowHomeButton),
                              prefs_->GetBoolean(prefs::kHomePageIsNewTabPage),
                              homepage_url);
#endif

  // Tab restoring is always done on Android, so these metrics are not
  // applicable.  Also, startup pages are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
  int restore_on_startup = prefs_->GetInteger(prefs::kRestoreOnStartup);
  UMA_HISTOGRAM_ENUMERATION(
      "Settings.StartupPageLoadSettings2", restore_on_startup,
      static_cast<int>(SessionStartupPref::kPrefValueMax));
  if (SessionStartupPref(
          SessionStartupPref::PrefValueToType(restore_on_startup))
          .ShouldOpenUrls()) {
    const base::Value::List& url_list =
        prefs_->GetList(prefs::kURLsToRestoreOnStartup);
    // Similarly, check startup pages for known search engine TLD+1s.
    for (const base::Value& i : url_list) {
      const std::string* url_text = i.GetIfString();
      if (url_text) {
        GURL start_url(*url_text);
        if (start_url.is_valid()) {
          UMA_HISTOGRAM_ENUMERATION("Settings.StartupPageEngineTypes2",
                                    SearchEngineUtils::GetEngineType(start_url),
                                    SEARCH_ENGINE_MAX);
        }
      }
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
    : ProfileKeyedServiceFactory(
          "PrefMetricsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // Not needed for any other profile types because these settings
              // is only configurable by the profile owner for regular
              // browser profiles.
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

PrefMetricsService::Factory::~Factory() {
}

std::unique_ptr<KeyedService>
PrefMetricsService::Factory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PrefMetricsService>(profile);
}

bool PrefMetricsService::Factory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
