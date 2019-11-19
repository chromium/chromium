// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_android.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/android/location_settings.h"
#include "chrome/browser/android/location_settings_impl.h"
#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

int g_day_offset_for_testing = 0;
const char* g_dse_origin_for_testing = nullptr;

const char kLocationSettingsShowMetricBase[] =
    "Geolocation.SettingsDialog.ShowEvent.";
const char kLocationSettingsSuppressMetricBase[] =
    "Geolocation.SettingsDialog.SuppressEvent.";
const char kLocationSettingsAcceptMetricBase[] =
    "Geolocation.SettingsDialog.AcceptEvent.";
const char kLocationSettingsDenyMetricBase[] =
    "Geolocation.SettingsDialog.DenyEvent.";
const char kLocationSettingsAcceptBatteryMetric[] =
    "Permissions.BatteryLevel.Accepted.LocationSettingsDialog";
const char kLocationSettingsDenyBatteryMetric[] =
    "Permissions.BatteryLevel.Denied.LocationSettingsDialog";

const char kLocationSettingsMetricDSESuffix[] = "DSE";
const char kLocationSettingsMetricNonDSESuffix[] = "NonDSE";

base::Time GetTimeNow() {
  return base::Time::Now() +
         base::TimeDelta::FromDays(g_day_offset_for_testing);
}

void LogLocationSettingsMetric(
    const std::string& metric_base,
    bool is_default_search,
    GeolocationPermissionContextAndroid::LocationSettingsDialogBackOff
        backoff) {
  std::string metric_name = metric_base;
  if (is_default_search)
    metric_name.append(kLocationSettingsMetricDSESuffix);
  else
    metric_name.append(kLocationSettingsMetricNonDSESuffix);

  base::UmaHistogramEnumeration(metric_name, backoff,
                                GeolocationPermissionContextAndroid::
                                    LocationSettingsDialogBackOff::kCount);
}

}  // namespace

// static
void GeolocationPermissionContextAndroid::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kLocationSettingsBackoffLevelDSE, 0);
  registry->RegisterIntegerPref(prefs::kLocationSettingsBackoffLevelDefault, 0);
  registry->RegisterInt64Pref(prefs::kLocationSettingsNextShowDSE, 0);
  registry->RegisterInt64Pref(prefs::kLocationSettingsNextShowDefault, 0);
}

GeolocationPermissionContextAndroid::GeolocationPermissionContextAndroid(
    Profile* profile)
    : GeolocationPermissionContext(profile),
      location_settings_(new LocationSettingsImpl()),
      location_settings_dialog_request_id_(0, 0, 0) {}

GeolocationPermissionContextAndroid::~GeolocationPermissionContextAndroid() {
}

// static
void GeolocationPermissionContextAndroid::AddDayOffsetForTesting(int days) {
  g_day_offset_for_testing += days;
}

// static
void GeolocationPermissionContextAndroid::SetDSEOriginForTesting(
    const char* dse_origin) {
  g_dse_origin_for_testing = dse_origin;
}

void GeolocationPermissionContextAndroid::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame_origin,
    bool user_gesture,
    BrowserPermissionCallback callback) {
  if (!IsLocationAccessPossible(web_contents, requesting_frame_origin,
                                user_gesture)) {
    NotifyPermissionSet(id, requesting_frame_origin,
                        web_contents->GetLastCommittedURL().GetOrigin(),
                        std::move(callback), false /* persist */,
                        CONTENT_SETTING_BLOCK);
    return;
  }

  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();
  ContentSetting content_setting =
      GeolocationPermissionContext::GetPermissionStatus(
          nullptr /* render_frame_host */, requesting_frame_origin,
          embedding_origin)
          .content_setting;
  std::vector<ContentSettingsType> content_settings_types;
  content_settings_types.push_back(ContentSettingsType::GEOLOCATION);
  if (content_setting == CONTENT_SETTING_ALLOW &&
      PermissionUpdateInfoBarDelegate::ShouldShowPermissionInfoBar(
          web_contents, content_settings_types) ==
          ShowPermissionInfoBarState::SHOW_PERMISSION_INFOBAR) {
    PermissionUpdateInfoBarDelegate::Create(
        web_contents, content_settings_types,
        base::BindOnce(&GeolocationPermissionContextAndroid ::
                           HandleUpdateAndroidPermissions,
                       weak_factory_.GetWeakPtr(), id, requesting_frame_origin,
                       embedding_origin, std::move(callback)));

    return;
  }

  GeolocationPermissionContext::RequestPermission(
      web_contents, id, requesting_frame_origin, user_gesture,
      std::move(callback));
}

void GeolocationPermissionContextAndroid::UserMadePermissionDecision(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting) {
  // If the user has accepted geolocation, reset the location settings dialog
  // backoff.
  if (content_setting == CONTENT_SETTING_ALLOW)
    ResetLocationSettingsBackOff(IsRequestingOriginDSE(requesting_origin));
}

void GeolocationPermissionContextAndroid::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting) {
  bool is_default_search = IsRequestingOriginDSE(requesting_origin);
  if (content_setting == CONTENT_SETTING_ALLOW &&
      !location_settings_->IsSystemLocationSettingEnabled()) {
    // There is no need to check CanShowLocationSettingsDialog here again, as it
    // must have been checked to get this far. But, the backoff will not have
    // been checked, so check that. Backoff isn't checked earlier because if the
    // content setting is ASK the backoff should be ignored. However if we get
    // here and the content setting was ASK, the user must have accepted which
    // would reset the backoff.
    if (IsInLocationSettingsBackOff(is_default_search)) {
      FinishNotifyPermissionSet(id, requesting_origin, embedding_origin,
                                std::move(callback), false /* persist */,
                                CONTENT_SETTING_BLOCK);
      LogLocationSettingsMetric(
          kLocationSettingsSuppressMetricBase, is_default_search,
          LocationSettingsBackOffLevel(is_default_search));
      return;
    }

    LogLocationSettingsMetric(kLocationSettingsShowMetricBase,
                              is_default_search,
                              LocationSettingsBackOffLevel(is_default_search));
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(id.render_process_id(),
                                             id.render_frame_id()));
    if (!web_contents)
      return;

    // Only show the location settings dialog if the tab for |web_contents| is
    // user-interactable (i.e. is the current tab, and Chrome is active and not
    // in tab-switching mode) and we're not already showing the LSD. The latter
    // case can occur in split-screen multi-window.
    TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
    if ((tab && !tab->IsUserInteractable()) ||
        !location_settings_dialog_callback_.is_null()) {
      FinishNotifyPermissionSet(id, requesting_origin, embedding_origin,
                                std::move(callback), false /* persist */,
                                CONTENT_SETTING_BLOCK);
      // This case should be very rare, so just pretend it was a denied prompt
      // for metrics purposes.
      LogLocationSettingsMetric(
          kLocationSettingsDenyMetricBase, is_default_search,
          LocationSettingsBackOffLevel(is_default_search));
      return;
    }

    location_settings_dialog_request_id_ = id;
    location_settings_dialog_callback_ = std::move(callback);
    location_settings_->PromptToEnableSystemLocationSetting(
        is_default_search ? SEARCH : DEFAULT, web_contents,
        base::BindOnce(
            &GeolocationPermissionContextAndroid::OnLocationSettingsDialogShown,
            weak_factory_.GetWeakPtr(), requesting_origin, embedding_origin,
            persist, content_setting));
    return;
  }

  FinishNotifyPermissionSet(id, requesting_origin, embedding_origin,
                            std::move(callback), persist, content_setting);
}

PermissionResult
GeolocationPermissionContextAndroid::UpdatePermissionStatusWithDeviceStatus(
    PermissionResult result,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (result.content_setting != CONTENT_SETTING_BLOCK) {
    if (!location_settings_->IsSystemLocationSettingEnabled()) {
      // As this is returning the status for possible future permission
      // requests, whose gesture status is unknown, pretend there is a user
      // gesture here. If there is a possibility of PROMPT (i.e. if there is a
      // user gesture attached to the later request) that should be returned,
      // not BLOCK.
      // If the permission is in the ASK state, backoff is ignored. Permission
      // prompts are shown regardless of backoff, if the location is off and the
      // LSD can be shown, as permission prompts are less annoying than the
      // modal LSD, and if the user accepts the permission prompt the LSD
      // backoff will be reset.
      if (CanShowLocationSettingsDialog(
              requesting_origin, true /* user_gesture */,
              result.content_setting ==
                  CONTENT_SETTING_ASK /* ignore_backoff */)) {
        result.content_setting = CONTENT_SETTING_ASK;
      } else {
        result.content_setting = CONTENT_SETTING_BLOCK;
      }
      result.source = PermissionStatusSource::UNSPECIFIED;
    }

    if (result.content_setting != CONTENT_SETTING_BLOCK &&
        !location_settings_->HasAndroidLocationPermission()) {
      // TODO(benwells): plumb through the RFH and use the associated
      // WebContents to check that the android location can be prompted for.
      result.content_setting = CONTENT_SETTING_ASK;
      result.source = PermissionStatusSource::UNSPECIFIED;
    }
  }

  return result;
}

std::string
GeolocationPermissionContextAndroid::GetLocationSettingsBackOffLevelPref(
    bool is_default_search) const {
  return is_default_search ? prefs::kLocationSettingsBackoffLevelDSE
                           : prefs::kLocationSettingsBackoffLevelDefault;
}

std::string
GeolocationPermissionContextAndroid::GetLocationSettingsNextShowPref(
    bool is_default_search) const {
  return is_default_search ? prefs::kLocationSettingsNextShowDSE
                           : prefs::kLocationSettingsNextShowDefault;
}

bool GeolocationPermissionContextAndroid::IsInLocationSettingsBackOff(
    bool is_default_search) const {
  base::Time next_show =
      base::Time::FromInternalValue(profile()->GetPrefs()->GetInt64(
          GetLocationSettingsNextShowPref(is_default_search)));

  return GetTimeNow() < next_show;
}

void GeolocationPermissionContextAndroid::ResetLocationSettingsBackOff(
    bool is_default_search) {
  PrefService* prefs = profile()->GetPrefs();
  prefs->ClearPref(GetLocationSettingsNextShowPref(is_default_search));
  prefs->ClearPref(GetLocationSettingsBackOffLevelPref(is_default_search));
}

void GeolocationPermissionContextAndroid::UpdateLocationSettingsBackOff(
    bool is_default_search) {
  LocationSettingsDialogBackOff backoff_level =
      LocationSettingsBackOffLevel(is_default_search);

  base::Time next_show = GetTimeNow();
  switch (backoff_level) {
    case LocationSettingsDialogBackOff::kNoBackOff:
      backoff_level = LocationSettingsDialogBackOff::kOneWeek;
      next_show += base::TimeDelta::FromDays(7);
      break;
    case LocationSettingsDialogBackOff::kOneWeek:
      backoff_level = LocationSettingsDialogBackOff::kOneMonth;
      next_show += base::TimeDelta::FromDays(30);
      break;
    case LocationSettingsDialogBackOff::kOneMonth:
      backoff_level = LocationSettingsDialogBackOff::kThreeMonths;
      FALLTHROUGH;
    case LocationSettingsDialogBackOff::kThreeMonths:
      next_show += base::TimeDelta::FromDays(90);
      break;
    default:
      NOTREACHED();
  }

  PrefService* prefs = profile()->GetPrefs();
  prefs->SetInteger(GetLocationSettingsBackOffLevelPref(is_default_search),
                    static_cast<int>(backoff_level));
  prefs->SetInt64(GetLocationSettingsNextShowPref(is_default_search),
                  next_show.ToInternalValue());
}

GeolocationPermissionContextAndroid::LocationSettingsDialogBackOff
GeolocationPermissionContextAndroid::LocationSettingsBackOffLevel(
    bool is_default_search) const {
  PrefService* prefs = profile()->GetPrefs();
  int int_backoff =
      prefs->GetInteger(GetLocationSettingsBackOffLevelPref(is_default_search));
  return static_cast<LocationSettingsDialogBackOff>(int_backoff);
}

bool GeolocationPermissionContextAndroid::IsLocationAccessPossible(
    content::WebContents* web_contents,
    const GURL& requesting_origin,
    bool user_gesture) {
  return (location_settings_->HasAndroidLocationPermission() ||
          location_settings_->CanPromptForAndroidLocationPermission(
              web_contents)) &&
         (location_settings_->IsSystemLocationSettingEnabled() ||
          CanShowLocationSettingsDialog(requesting_origin, user_gesture,
                                        true /* ignore_backoff */));
}

bool GeolocationPermissionContextAndroid::IsRequestingOriginDSE(
    const GURL& requesting_origin) const {
  GURL dse_url;

  if (g_dse_origin_for_testing) {
    dse_url = GURL(g_dse_origin_for_testing);
  } else {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    if (template_url_service) {
      const TemplateURL* template_url =
          template_url_service->GetDefaultSearchProvider();
      if (template_url) {
        dse_url = template_url->GenerateSearchURL(
            template_url_service->search_terms_data());
      }
    }
  }

  return url::IsSameOriginWith(requesting_origin, dse_url);
}

void GeolocationPermissionContextAndroid::HandleUpdateAndroidPermissions(
    const PermissionRequestID& id,
    const GURL& requesting_frame_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool permissions_updated) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ContentSetting new_setting = permissions_updated
      ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;

  NotifyPermissionSet(id, requesting_frame_origin, embedding_origin,
                      std::move(callback), false /* persist */, new_setting);
}

bool GeolocationPermissionContextAndroid::CanShowLocationSettingsDialog(
    const GURL& requesting_origin,
    bool user_gesture,
    bool ignore_backoff) const {
  bool is_default_search = IsRequestingOriginDSE(requesting_origin);
  // If this isn't the default search engine, a gesture is needed.
  if (!is_default_search && !user_gesture) {
    return false;
  }

  if (!ignore_backoff && IsInLocationSettingsBackOff(is_default_search))
    return false;

  return location_settings_->CanPromptToEnableSystemLocationSetting();
}

void GeolocationPermissionContextAndroid::OnLocationSettingsDialogShown(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool persist,
    ContentSetting content_setting,
    LocationSettingsDialogOutcome prompt_outcome) {
  bool is_default_search = IsRequestingOriginDSE(requesting_origin);
  if (prompt_outcome == GRANTED) {
    LogLocationSettingsMetric(kLocationSettingsAcceptMetricBase,
                              is_default_search,
                              LocationSettingsBackOffLevel(is_default_search));
    PermissionUmaUtil::RecordWithBatteryBucket(
        kLocationSettingsAcceptBatteryMetric);
    ResetLocationSettingsBackOff(is_default_search);
  } else {
    LogLocationSettingsMetric(kLocationSettingsDenyMetricBase,
                              is_default_search,
                              LocationSettingsBackOffLevel(is_default_search));
    PermissionUmaUtil::RecordWithBatteryBucket(
        kLocationSettingsDenyBatteryMetric);
    UpdateLocationSettingsBackOff(is_default_search);
    content_setting = CONTENT_SETTING_BLOCK;
    persist = false;
  }

  // If the permission was cancelled while the LSD was up, the callback has
  // already been dropped.
  if (!location_settings_dialog_callback_)
    return;

  FinishNotifyPermissionSet(
      location_settings_dialog_request_id_, requesting_origin, embedding_origin,
      std::move(location_settings_dialog_callback_), persist, content_setting);

  location_settings_dialog_request_id_ = PermissionRequestID(0, 0, 0);
}

void GeolocationPermissionContextAndroid::FinishNotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting) {
  GeolocationPermissionContext::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting);

  // If this is the default search origin, and the DSE Geolocation setting is
  // being used, potentially show the disclosure.
  if (requesting_origin == embedding_origin) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(id.render_process_id(),
                                             id.render_frame_id()));
    if (!web_contents)
      return;

    SearchGeolocationDisclosureTabHelper* disclosure_helper =
        SearchGeolocationDisclosureTabHelper::FromWebContents(web_contents);

    // The tab helper can be null in tests.
    if (disclosure_helper)
      disclosure_helper->MaybeShowDisclosureForAPIAccess(requesting_origin);
  }
}

void GeolocationPermissionContextAndroid::SetLocationSettingsForTesting(
    std::unique_ptr<LocationSettings> settings) {
  location_settings_ = std::move(settings);
}
