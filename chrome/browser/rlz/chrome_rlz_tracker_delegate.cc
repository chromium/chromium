// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
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
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/content_switches.h"
#include "rlz/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_WIN)
#include "chrome/installer/util/google_update_settings.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chromeos/constants/chromeos_switches.h"
#endif

ChromeRLZTrackerDelegate::ChromeRLZTrackerDelegate() {}

ChromeRLZTrackerDelegate::~ChromeRLZTrackerDelegate() {}

// static
void ChromeRLZTrackerDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(ENABLE_RLZ)
  int rlz_ping_delay_seconds = 90;
#if defined(OS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kRlzPingDelay)) {
    // Use a switch for overwriting the default delay because it doesn't seem
    // possible to manually override the Preferences file on Chrome OS: the file
    // is already loaded into memory by the time you modify it and any changes
    // made get overwritten by Chrome.
    rlz_ping_delay_seconds =
        std::stoi(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            chromeos::switches::kRlzPingDelay));
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
    is_google_in_startpages =
        std::count_if(session_startup_prefs.urls.begin(),
                      session_startup_prefs.urls.end(),
                      google_util::IsGoogleHomePageUrl) > 0;
  }
  return is_google_in_startpages;
}

void ChromeRLZTrackerDelegate::Cleanup() {
  registrar_.RemoveAll();
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

bool ChromeRLZTrackerDelegate::GetLanguage(base::string16* language) {
#if defined(OS_WIN)
  return GoogleUpdateSettings::GetLanguage(language);
#else
  // On other systems, we don't know the install language of promotions. That's
  // OK, for now all promotions on non-Windows systems will be reported as "en".
  // If non-Windows promotions end up requiring language code reporting, that
  // code will need to go here.
  return false;
#endif
}

bool ChromeRLZTrackerDelegate::GetReferral(base::string16* referral) {
#if defined(OS_WIN)
  return GoogleUpdateSettings::GetReferral(referral);
#else
  // The referral program is defunct and not used. No need to implement this
  // function on non-Win platforms.
  return true;
#endif
}

bool ChromeRLZTrackerDelegate::ClearReferral() {
#if defined(OS_WIN)
  return GoogleUpdateSettings::ClearReferral();
#else
  // The referral program is defunct and not used. No need to implement this
  // function on non-Win platforms.
  return true;
#endif
}

void ChromeRLZTrackerDelegate::SetOmniboxSearchCallback(
    const base::Closure& callback) {
  DCHECK(!callback.is_null());
  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::Bind(&ChromeRLZTrackerDelegate::OnURLOpenedFromOmnibox,
                     base::Unretained(this)));
  on_omnibox_search_callback_ = callback;
}

void ChromeRLZTrackerDelegate::SetHomepageSearchCallback(
    const base::Closure& callback) {
  DCHECK(!callback.is_null());
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
  on_homepage_search_callback_ = callback;
}

bool ChromeRLZTrackerDelegate::ShouldUpdateExistingAccessPointRlz() {
  return true;
}

void ChromeRLZTrackerDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  using std::swap;
  base::Closure callback_to_run;
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      // Firstly check if it is a Google search.
      content::LoadCommittedDetails* load_details =
          content::Details<content::LoadCommittedDetails>(details).ptr();
      if (load_details == nullptr)
        break;

      content::NavigationEntry* entry = load_details->entry;
      if (entry == nullptr)
        break;

      if (google_util::IsGoogleSearchUrl(entry->GetURL())) {
        // If it is a Google search, check if it originates from HOMEPAGE by
        // getting the previous NavigationEntry.
        content::NavigationController* controller =
            content::Source<content::NavigationController>(source).ptr();
        if (controller == nullptr)
          break;

        int entry_index = controller->GetLastCommittedEntryIndex();
        if (entry_index < 1)
          break;

        content::NavigationEntry* previous_entry =
            controller->GetEntryAtIndex(entry_index - 1);

        if (previous_entry == nullptr)
          break;

        // Make sure it is a Google web page originated from HOMEPAGE.
        if (google_util::IsGoogleHomePageUrl(previous_entry->GetURL()) &&
            ((previous_entry->GetTransitionType() &
              ui::PAGE_TRANSITION_HOME_PAGE) != 0)) {
          registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                            content::NotificationService::AllSources());
          swap(callback_to_run, on_homepage_search_callback_);
        }
      }
      break;
    }

    default:
      NOTREACHED();
      break;
  }

  if (!callback_to_run.is_null())
    callback_to_run.Run();
}

void ChromeRLZTrackerDelegate::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  using std::swap;

  // In M-36, we made NOTIFICATION_OMNIBOX_OPENED_URL fire more often than
  // it did previously.  The RLZ folks want RLZ's "first search" detection
  // to remain as unaffected as possible by this change.  This test is
  // there to keep the old behavior.
  if (!log->is_popup_open)
    return;

  omnibox_url_opened_subscription_.reset();
  base::Closure omnibox_callback;
  swap(omnibox_callback, on_omnibox_search_callback_);
  omnibox_callback.Run();
}
