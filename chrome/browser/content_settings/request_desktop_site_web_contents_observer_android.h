// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_REQUEST_DESKTOP_SITE_WEB_CONTENTS_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_REQUEST_DESKTOP_SITE_WEB_CONTENTS_OBSERVER_ANDROID_H_

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// This observer is Android-specific and intercepts each navigation on the main
// frame. For sites configured so, it overrides the user agent to request the
// desktop version of the site.
// TODO(crbug.com/40856033): Add tests for this class.
class RequestDesktopSiteWebContentsObserverAndroid
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          RequestDesktopSiteWebContentsObserverAndroid> {
 public:
  RequestDesktopSiteWebContentsObserverAndroid(
      const RequestDesktopSiteWebContentsObserverAndroid&) = delete;
  RequestDesktopSiteWebContentsObserverAndroid& operator=(
      const RequestDesktopSiteWebContentsObserverAndroid&) = delete;

  ~RequestDesktopSiteWebContentsObserverAndroid() override;

  // content::WebContentsObserver implementation.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit RequestDesktopSiteWebContentsObserverAndroid(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      RequestDesktopSiteWebContentsObserverAndroid>;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  raw_ptr<PrefService> pref_service_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_REQUEST_DESKTOP_SITE_WEB_CONTENTS_OBSERVER_ANDROID_H_
