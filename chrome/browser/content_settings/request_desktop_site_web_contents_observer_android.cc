// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/request_desktop_site_web_contents_observer_android.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"

RequestDesktopSiteWebContentsObserverAndroid::
    RequestDesktopSiteWebContentsObserverAndroid(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<
          RequestDesktopSiteWebContentsObserverAndroid>(*contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (base::FeatureList::IsEnabled(features::kRequestDesktopSiteAdditions)) {
    pref_service_ = profile->GetPrefs();
    tab_android_ = TabAndroid::FromWebContents(contents);
  }
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
  // Override UA for renderer initiated navigation only. UA override for browser
  // initiated navigation is handled on Java side. This is to workaround known
  // issues crbug.com/1265751 and crbug.com/1261939.
  if (!navigation_handle->IsRendererInitiated()) {
    return;
  }
  if (!base::FeatureList::IsEnabled(features::kRequestDesktopSiteExceptions)) {
    // Stop UA override if there is a tab level setting.
    TabModel::TabUserAgent tabSetting =
        tab_android_
            ? static_cast<TabModel::TabUserAgent>(tab_android_->GetUserAgent())
            : TabModel::TabUserAgent::DEFAULT;
    if (tabSetting != TabModel::TabUserAgent::DEFAULT) {
      return;
    }
  }

  const GURL& url = navigation_handle->GetParentFrameOrOuterDocument()
                        ? navigation_handle->GetParentFrameOrOuterDocument()
                              ->GetOutermostMainFrame()
                              ->GetLastCommittedURL()
                        : navigation_handle->GetURL();
  content_settings::SettingInfo setting_info;
  const base::Value setting = host_content_settings_map_->GetWebsiteSetting(
      url, url, ContentSettingsType::REQUEST_DESKTOP_SITE, &setting_info);
  bool use_rds =
      content_settings::ValueToContentSetting(setting) == CONTENT_SETTING_ALLOW;

  // Take secondary settings into account if ContentSetting is global setting.
  if (!use_rds &&
      base::FeatureList::IsEnabled(features::kRequestDesktopSiteAdditions) &&
      setting_info.primary_pattern.MatchesAllHosts()) {
    bool use_rds_peripheral =
        pref_service_->GetBoolean(prefs::kDesktopSitePeripheralSettingEnabled);
    if (use_rds_peripheral) {
      use_rds = TabAndroid::isHardwareKeyboardAvailable(tab_android_);
    }
  }
  navigation_handle->SetIsOverridingUserAgent(use_rds);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(RequestDesktopSiteWebContentsObserverAndroid);
