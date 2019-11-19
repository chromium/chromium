// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_tab_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/GeolocationHeader_jni.h"
#include "chrome/android/chrome_jni_headers/SearchGeolocationDisclosureTabHelper_jni.h"
#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_infobar_delegate.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/origin.h"

namespace {

const int kMaxShowCount = 3;
const int kDaysPerShow = 1;

bool gIgnoreUrlChecksForTesting = false;
int gDayOffsetForTesting = 0;

base::Time GetTimeNow() {
  return base::Time::Now() + base::TimeDelta::FromDays(gDayOffsetForTesting);
}

}  // namespace

SearchGeolocationDisclosureTabHelper::SearchGeolocationDisclosureTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {}

SearchGeolocationDisclosureTabHelper::~SearchGeolocationDisclosureTabHelper() {}

void SearchGeolocationDisclosureTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  MaybeShowDisclosureForNavigation(web_contents()->GetVisibleURL());
}

void SearchGeolocationDisclosureTabHelper::MaybeShowDisclosureForAPIAccess(
    const GURL& gurl) {
  if (!ShouldShowDisclosureForAPIAccess(gurl))
    return;

  MaybeShowDisclosureForValidUrl(gurl);
}

// static
void SearchGeolocationDisclosureTabHelper::ResetDisclosure(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  prefs->ClearPref(prefs::kSearchGeolocationDisclosureShownCount);
  prefs->ClearPref(prefs::kSearchGeolocationDisclosureLastShowDate);
  prefs->ClearPref(prefs::kSearchGeolocationDisclosureDismissed);
}

// static
void SearchGeolocationDisclosureTabHelper::FakeShowingDisclosureForTests(
    Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  prefs->SetInteger(prefs::kSearchGeolocationDisclosureShownCount, 1);
}

// static
bool SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
    Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  return !prefs->HasPrefPath(prefs::kSearchGeolocationDisclosureShownCount);
}

// static
void SearchGeolocationDisclosureTabHelper::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSearchGeolocationDisclosureDismissed,
                                false);
  registry->RegisterIntegerPref(prefs::kSearchGeolocationDisclosureShownCount,
                                0);
  registry->RegisterInt64Pref(prefs::kSearchGeolocationDisclosureLastShowDate,
                              0);
  registry->RegisterBooleanPref(
      prefs::kSearchGeolocationPreDisclosureMetricsRecorded, false);
  registry->RegisterBooleanPref(
      prefs::kSearchGeolocationPostDisclosureMetricsRecorded, false);
}

void SearchGeolocationDisclosureTabHelper::MaybeShowDisclosureForNavigation(
    const GURL& gurl) {
  if (!ShouldShowDisclosureForNavigation(gurl))
    return;

  MaybeShowDisclosureForValidUrl(gurl);
}

void SearchGeolocationDisclosureTabHelper::MaybeShowDisclosureForValidUrl(
    const GURL& gurl) {
  // Don't show the infobar if the user has dismissed it, or they've seen it
  // enough times already.
  PrefService* prefs = GetProfile()->GetPrefs();
  bool dismissed_already =
      prefs->GetBoolean(prefs::kSearchGeolocationDisclosureDismissed);
  int shown_count =
      prefs->GetInteger(prefs::kSearchGeolocationDisclosureShownCount);
  if (dismissed_already || shown_count >= kMaxShowCount) {
    // Record metrics for the state of permissions after the disclosure has been
    // shown. This is not done immediately after showing the last disclosure
    // (i.e. at the end of this function), but on the next omnibox search, to
    // allow the metric to capture changes to settings done by the user as a
    // result of clicking on the Settings link in the disclosure.
    RecordPostDisclosureMetrics(gurl);
    return;
  }

  // Or if it has been shown too recently.
  base::Time last_shown = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSearchGeolocationDisclosureLastShowDate));
  if (GetTimeNow() - last_shown < base::TimeDelta::FromDays(kDaysPerShow)) {
    return;
  }

  // Record metrics for the state of permissions before the disclosure has been
  // shown.
  RecordPreDisclosureMetrics(gurl);

  // Only show disclosure if the DSE geolocation setting is on.
  if (PermissionManager::Get(GetProfile())
          ->GetPermissionStatus(ContentSettingsType::GEOLOCATION, gurl, gurl)
          .content_setting != CONTENT_SETTING_ALLOW) {
    return;
  }

  // Check that the Chrome app has geolocation permission.
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_GeolocationHeader_hasGeolocationPermission(env))
    return;

  // All good, let's show the disclosure and increment the shown count.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(GetProfile());
  const TemplateURL* template_url =
      template_url_service->GetDefaultSearchProvider();
  // ShouldShowDisclosureForNavigation() checked explicitly that the default
  // search |template_url| was non-null, and ShouldShowDisclosureForAPIAccess()
  // would have also seen an empty DSE origin if it were.
  DCHECK(template_url);
  base::string16 search_engine_name = template_url->short_name();
  SearchGeolocationDisclosureInfoBarDelegate::Create(web_contents(), gurl,
                                                     search_engine_name);
  shown_count++;
  prefs->SetInteger(prefs::kSearchGeolocationDisclosureShownCount, shown_count);
  prefs->SetInt64(prefs::kSearchGeolocationDisclosureLastShowDate,
                  GetTimeNow().ToInternalValue());
}

bool SearchGeolocationDisclosureTabHelper::ShouldShowDisclosureForAPIAccess(
    const GURL& gurl) {
  SearchPermissionsService* service =
      SearchPermissionsService::Factory::GetForBrowserContext(GetProfile());

  // Check the service first, as we don't want to show the infobar even when
  // testing if it does not exist.
  if (!service)
    return false;

  if (gIgnoreUrlChecksForTesting)
    return true;

  return service->IsPermissionControlledByDSE(ContentSettingsType::GEOLOCATION,
                                              url::Origin::Create(gurl));
}

bool SearchGeolocationDisclosureTabHelper::ShouldShowDisclosureForNavigation(
    const GURL& gurl) {
  if (!ShouldShowDisclosureForAPIAccess(gurl))
    return false;

  if (gIgnoreUrlChecksForTesting)
    return true;

  // Only show the disclosure for default search navigations from the omnibox,
  // and only if they are for the Google search engine (only Google supports the
  // X-Geo header).
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(GetProfile());
  const TemplateURL* template_url =
      template_url_service->GetDefaultSearchProvider();
  return template_url &&
         template_url->HasGoogleBaseURLs(UIThreadSearchTermsData()) &&
         template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
             gurl);
}

void SearchGeolocationDisclosureTabHelper::RecordPreDisclosureMetrics(
    const GURL& gurl) {
  PrefService* prefs = GetProfile()->GetPrefs();
  if (!prefs->GetBoolean(
          prefs::kSearchGeolocationPreDisclosureMetricsRecorded)) {
    ContentSetting status =
        HostContentSettingsMapFactory::GetForProfile(GetProfile())
            ->GetContentSetting(gurl, gurl, ContentSettingsType::GEOLOCATION,
                                std::string());

    UMA_HISTOGRAM_BOOLEAN("GeolocationDisclosure.PreDisclosureDSESetting",
                          status == CONTENT_SETTING_ALLOW);

    prefs->SetBoolean(prefs::kSearchGeolocationPreDisclosureMetricsRecorded,
                      true);
  }
}

void SearchGeolocationDisclosureTabHelper::RecordPostDisclosureMetrics(
    const GURL& gurl) {
  PrefService* prefs = GetProfile()->GetPrefs();
  if (!prefs->GetBoolean(
          prefs::kSearchGeolocationPostDisclosureMetricsRecorded)) {
    ContentSetting status =
        HostContentSettingsMapFactory::GetForProfile(GetProfile())
            ->GetContentSetting(gurl, gurl, ContentSettingsType::GEOLOCATION,
                                std::string());

    UMA_HISTOGRAM_BOOLEAN("GeolocationDisclosure.PostDisclosureDSESetting",
                          status == CONTENT_SETTING_ALLOW);

    prefs->SetBoolean(prefs::kSearchGeolocationPostDisclosureMetricsRecorded,
                      true);
  }
}

Profile* SearchGeolocationDisclosureTabHelper::GetProfile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

// static
void JNI_SearchGeolocationDisclosureTabHelper_SetIgnoreUrlChecksForTesting(
    JNIEnv* env) {
  gIgnoreUrlChecksForTesting = true;
}

// static
void JNI_SearchGeolocationDisclosureTabHelper_SetDayOffsetForTesting(
    JNIEnv* env,
    jint days) {
  gDayOffsetForTesting = days;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchGeolocationDisclosureTabHelper)
