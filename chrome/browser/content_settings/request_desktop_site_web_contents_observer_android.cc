// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/request_desktop_site_web_contents_observer_android.h"

#include "base/android/device_info.h"
#include "base/command_line.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/preferences/android/chrome_shared_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/display/screen.h"

namespace rds_web_contents_observer {
// Keep in sync with UserAgentRequestType in tools/metrics/histograms/enums.xml.
enum class UserAgentRequestType {
  RequestDesktop = 0,
  RequestMobile = 1,
};
}  // namespace rds_web_contents_observer

RequestDesktopSiteWebContentsObserverAndroid::
    RequestDesktopSiteWebContentsObserverAndroid(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<
          RequestDesktopSiteWebContentsObserverAndroid>(*contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile);
  pref_service_ = profile->GetPrefs();
}

RequestDesktopSiteWebContentsObserverAndroid::
    ~RequestDesktopSiteWebContentsObserverAndroid() = default;

void RequestDesktopSiteWebContentsObserverAndroid::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // A webpage could contain multiple frames, which will trigger this observer
  // multiple times. Only need to override user agent for the main frame of the
  // webpage; since the child iframes inherit from the main frame.
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  const GURL& url = navigation_handle->GetParentFrameOrOuterDocument()
                        ? navigation_handle->GetParentFrameOrOuterDocument()
                              ->GetOutermostMainFrame()
                              ->GetLastCommittedURL()
                        : navigation_handle->GetURL();
  content_settings::SettingInfo setting_info;
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      url, url, ContentSettingsType::REQUEST_DESKTOP_SITE, &setting_info);
  bool desktop_mode = setting == CONTENT_SETTING_ALLOW;
  // For --request-desktop-sites, always override the user agent.
  bool always_request_desktop_site =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRequestDesktopSites);
  desktop_mode |= always_request_desktop_site;
  bool is_global_setting = setting_info.primary_pattern.MatchesAllHosts();

  // RDS Window Setting support.
  if (!base::android::device_info::is_automotive() &&
      pref_service_->GetBoolean(prefs::kDesktopSiteWindowSettingEnabled) &&
      desktop_mode && !always_request_desktop_site && is_global_setting) {
    int web_contents_width_dp =
        web_contents()->GetContainerBounds().size().width();
    if (web_contents_width_dp > 0 &&
        web_contents_width_dp < content::kAndroidMinimumTabletWidthDp) {
      desktop_mode = false;
    }
  }

  base::android::SharedPreferencesManager shared_prefs =
      android::shared_preferences::GetChromeSharedPreferences();
  // Enable on large connected displays only when user has not explicitly set
  // preference. ie: user is using global setting and has not changed it.
  display::Display display = display::Screen::Get()->GetDisplayNearestWindow(
      web_contents()->GetTopLevelNativeWindow());
  // Compute the display's diagonal length in inches.
  float width_inches = static_cast<float>(display.GetSizeInPixel().width()) /
                       display.GetPixelsPerInchX();
  float height_inches = static_cast<float>(display.GetSizeInPixel().height()) /
                        display.GetPixelsPerInchY();
  double diagonal_inches =
      std::sqrt(std::pow(width_inches, 2) + std::pow(height_inches, 2));
  bool is_on_eligible_external_display =
      display.id() != display::kDefaultDisplayId &&
      diagonal_inches >= kDesktopSiteDisplaySizeThresholdInches;
  bool should_allow_on_external_display =
      base::FeatureList::IsEnabled(
          chrome::android::kDesktopUAOnConnectedDisplay) &&
      is_on_eligible_external_display && is_global_setting &&
      !shared_prefs.ContainsKey(
          prefs::kRequestDesktopSiteGlobalSettingUserEnabled);
  desktop_mode |= should_allow_on_external_display;

  // Override UA for renderer initiated navigation only. UA override for browser
  // initiated navigation is handled on Java side. This is to workaround known
  // issues crbug.com/1265751 and crbug.com/1261939.
  if (navigation_handle->IsRendererInitiated()) {
    navigation_handle->SetIsOverridingUserAgent(desktop_mode);
  }

  // Only record UKM for site settings and primary main frame.
  if (is_global_setting || !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  rds_web_contents_observer::UserAgentRequestType user_agent_request_type;
  if (desktop_mode) {
    user_agent_request_type =
        rds_web_contents_observer::UserAgentRequestType::RequestDesktop;
  } else {
    user_agent_request_type =
        rds_web_contents_observer::UserAgentRequestType::RequestMobile;
  }
  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::Android_NonDefaultRdsPageLoad(source_id)
      .SetUserAgentType(static_cast<int>(user_agent_request_type))
      .Record(ukm::UkmRecorder::Get());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(RequestDesktopSiteWebContentsObserverAndroid);
