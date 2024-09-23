// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_NAVIGATION_METRICS_RECORDER_H_
#define CHROME_BROWSER_TAB_CONTENTS_NAVIGATION_METRICS_RECORDER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;

namespace site_engagement {
class SiteEngagementService;
}

// Whether third-party cookie blocking was enabled for this pageload.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ThirdPartyCookieBlockState {
  kCookiesAllowed = 0,
  kThirdPartyCookiesBlocked = 1,
  kThirdPartyCookieBlockingDisabledForSite = 2,
  kMaxValue = kThirdPartyCookieBlockingDisabledForSite,
};

class NavigationMetricsRecorder
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NavigationMetricsRecorder> {
 public:
  NavigationMetricsRecorder(const NavigationMetricsRecorder&) = delete;
  NavigationMetricsRecorder& operator=(const NavigationMetricsRecorder&) =
      delete;

  ~NavigationMetricsRecorder() override;
 private:
  explicit NavigationMetricsRecorder(content::WebContents* web_contents);
  friend class content::WebContentsUserData<NavigationMetricsRecorder>;

  ThirdPartyCookieBlockState GetThirdPartyCookieBlockState(const GURL& url);

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  raw_ptr<site_engagement::SiteEngagementService> site_engagement_service_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_NAVIGATION_METRICS_RECORDER_H_
