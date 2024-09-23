// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/prefs/pref_metrics_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/LaunchMetrics_jni.h"

using base::android::JavaParamRef;

namespace metrics {

enum class HomeScreenLaunchType { STANDALONE = 0, SHORTCUT = 1, COUNT = 2 };

static void JNI_LaunchMetrics_RecordLaunch(
    JNIEnv* env,
    jboolean is_shortcut,
    const JavaParamRef<jstring>& jurl,
    int source,
    int display_mode,
    const JavaParamRef<jobject>& jweb_contents) {
  // Interpolate the legacy ADD_TO_HOMESCREEN source into standalone/shortcut.
  // Unfortunately, we cannot concretely determine whether a standalone add to
  // homescreen source means a full PWA (with service worker) or a site that has
  // a manifest with display: standalone.
  int histogram_source = source;
  if (histogram_source ==
      webapps::ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_DEPRECATED) {
    if (is_shortcut)
      histogram_source =
          webapps::ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_SHORTCUT;
    else
      histogram_source =
          webapps::ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_STANDALONE;
  }

  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  if (web_contents && (histogram_source ==
                       webapps::ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_PWA)) {
    // Tell the Site Engagement Service about this launch as sites recently
    // launched from a shortcut receive a boost to their engagement.
    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(
            Profile::FromBrowserContext(web_contents->GetBrowserContext()));
    service->SetLastShortcutLaunchTime(web_contents, url);
  }

  base::UmaHistogramEnumeration(
      "Launch.HomeScreenSource",
      static_cast<webapps::ShortcutInfo::Source>(histogram_source),
      webapps::ShortcutInfo::SOURCE_COUNT);

  if (!is_shortcut) {
    base::UmaHistogramEnumeration(
        "Launch.WebAppDisplayMode",
        static_cast<blink::mojom::DisplayMode>(display_mode));
  } else {
    base::UmaHistogramEnumeration(
        "Launch.Window.CreateShortcutApp.WebAppDisplayMode",
        static_cast<blink::mojom::DisplayMode>(display_mode));
  }

  HomeScreenLaunchType action = is_shortcut ? HomeScreenLaunchType::SHORTCUT
                                            : HomeScreenLaunchType::STANDALONE;

  base::UmaHistogramEnumeration("Launch.HomeScreen", action,
                                HomeScreenLaunchType::COUNT);
}

static void JNI_LaunchMetrics_RecordHomePageLaunchMetrics(
    JNIEnv* env,
    jboolean show_home_button,
    jboolean homepage_is_ntp,
    const JavaParamRef<jobject>& jhomepage_gurl) {
  GURL homepage_gurl = url::GURLAndroid::ToNativeGURL(env, jhomepage_gurl);
  PrefMetricsService::RecordHomePageLaunchMetrics(
      show_home_button, homepage_is_ntp, homepage_gurl);
}

}  // namespace metrics
