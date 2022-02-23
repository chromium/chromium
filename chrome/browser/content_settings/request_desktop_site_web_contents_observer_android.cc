// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/request_desktop_site_web_contents_observer_android.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"

RequestDesktopSiteWebContentsObserverAndroid::
    RequestDesktopSiteWebContentsObserverAndroid(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<
          RequestDesktopSiteWebContentsObserverAndroid>(*contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile);
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

  const GURL& url = navigation_handle->GetURL();
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      url, url, ContentSettingsType::REQUEST_DESKTOP_SITE);
  bool use_rds = setting == CONTENT_SETTING_ALLOW;
  navigation_handle->SetIsOverridingUserAgent(use_rds);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(RequestDesktopSiteWebContentsObserverAndroid);
