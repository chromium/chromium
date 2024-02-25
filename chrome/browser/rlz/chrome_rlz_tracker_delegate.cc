// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "rlz/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/google_update_settings.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

ChromeRLZTrackerDelegate::ChromeRLZTrackerDelegate() {}

ChromeRLZTrackerDelegate::~ChromeRLZTrackerDelegate() {}

// static
void ChromeRLZTrackerDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(ENABLE_RLZ)
  int rlz_ping_delay_seconds = 90;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kRlzPingDelay)) {
    // Use a switch for overwriting the default delay because it doesn't seem
    // possible to manually override the Preferences file on Chrome OS: the file
    // is already loaded into memory by the time you modify it and any changes
    // made get overwritten by Chrome.
    int parsed_delay_from_switch = 0;
    if (base::StringToInt(
            base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                ash::switches::kRlzPingDelay),
            &parsed_delay_from_switch)) {
      rlz_ping_delay_seconds = parsed_delay_from_switch;
    }
  } else {
    rlz_ping_delay_seconds = 24 * 3600;
  }
#endif
  registry->RegisterIntegerPref(prefs::kRlzPingDelaySeconds,
                                rlz_ping_delay_seconds);
#endif
}

// static
bool ChromeRLZTrackerDelegate::IsGoogleDefaultSearch(Profile* profile) {
  bool is_google_default_search = false;
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (template_url_service) {
    const TemplateURL* url_template =
        template_url_service->GetDefaultSearchProvider();
    is_google_default_search = url_template &&
                               url_template->url_ref().HasGoogleBaseURLs(
                                   template_url_service->search_terms_data());
  }
  return is_google_default_search;
}

// static
bool ChromeRLZTrackerDelegate::IsGoogleHomepage(Profile* profile) {
  return google_util::IsGoogleHomePageUrl(
      GURL(profile->GetPrefs()->GetString(prefs::kHomePage)));
}

// static
bool ChromeRLZTrackerDelegate::IsGoogleInStartpages(Profile* profile) {
  bool is_google_in_startpages = false;
  SessionStartupPref session_startup_prefs =
      StartupBrowserCreator::GetSessionStartupPref(
          *base::CommandLine::ForCurrentProcess(), profile);
  if (session_startup_prefs.type == SessionStartupPref::URLS) {
    is_google_in_startpages = base::Contains(session_startup_prefs.urls, true,
                                             google_util::IsGoogleHomePageUrl);
  }
  return is_google_in_startpages;
}

void ChromeRLZTrackerDelegate::Cleanup() {
  on_omnibox_search_callback_.Reset();
  on_homepage_search_callback_.Reset();
}

bool ChromeRLZTrackerDelegate::IsOnUIThread() {
  return content::BrowserThread::CurrentlyOn(content::BrowserThread::UI);
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeRLZTrackerDelegate::GetURLLoaderFactory() {
  return g_browser_process->shared_url_loader_factory();
}

bool ChromeRLZTrackerDelegate::GetBrand(std::string* brand) {
  return google_brand::GetRlzBrand(brand);
}

bool ChromeRLZTrackerDelegate::IsBrandOrganic(const std::string& brand) {
  return google_brand::IsOrganic(brand);
}

bool ChromeRLZTrackerDelegate::GetReactivationBrand(std::string* brand) {
  return google_brand::GetReactivationBrand(brand);
}

bool ChromeRLZTrackerDelegate::ShouldEnableZeroDelayForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kTestType);
}

bool ChromeRLZTrackerDelegate::GetLanguage(std::u16string* language) {
#if BUILDFLAG(IS_WIN)
  std::wstring wide_language;
  bool result = GoogleUpdateSettings::GetLanguage(&wide_language);
  *language = base::AsString16(wide_language);
  return result;
#else
  // On other systems, we don't know the install language of promotions. That's
  // OK, for now all promotions on non-Windows systems will be reported as "en".
  // If non-Windows promotions end up requiring language code reporting, that
  // code will need to go here.
  return false;
#endif
}

bool ChromeRLZTrackerDelegate::GetReferral(std::u16string* referral) {
#if BUILDFLAG(IS_WIN)
  std::wstring wide_referral;
  bool result = GoogleUpdateSettings::GetReferral(&wide_referral);
  *referral = base::AsString16(wide_referral);
  return result;
#else
  // The referral program is defunct and not used. No need to implement this
  // function on non-Win platforms.
  return true;
#endif
}

bool ChromeRLZTrackerDelegate::ClearReferral() {
#if BUILDFLAG(IS_WIN)
  return GoogleUpdateSettings::ClearReferral();
#else
  // The referral program is defunct and not used. No need to implement this
  // function on non-Win platforms.
  return true;
#endif
}

void ChromeRLZTrackerDelegate::SetOmniboxSearchCallback(
    base::OnceClosure callback) {
  DCHECK(!callback.is_null());
  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::BindRepeating(&ChromeRLZTrackerDelegate::OnURLOpenedFromOmnibox,
                              base::Unretained(this)));
  on_omnibox_search_callback_ = std::move(callback);
}

void ChromeRLZTrackerDelegate::SetHomepageSearchCallback(
    base::OnceClosure callback) {
  DCHECK(!callback.is_null());
  on_homepage_search_callback_ = std::move(callback);
}

void ChromeRLZTrackerDelegate::RunHomepageSearchCallback() {
  if (!on_homepage_search_callback_.is_null()) {
    std::move(on_homepage_search_callback_).Run();
  }
}

bool ChromeRLZTrackerDelegate::ShouldUpdateExistingAccessPointRlz() {
  return true;
}

void ChromeRLZTrackerDelegate::OnURLOpenedFromOmnibox(OmniboxLog* log) {

  // In M-36, we made NOTIFICATION_OMNIBOX_OPENED_URL fire more often than
  // it did previously.  The RLZ folks want RLZ's "first search" detection
  // to remain as unaffected as possible by this change.  This test is
  // there to keep the old behavior.
  if (!log->is_popup_open)
    return;

  omnibox_url_opened_subscription_ = {};
  std::move(on_omnibox_search_callback_).Run();
}
