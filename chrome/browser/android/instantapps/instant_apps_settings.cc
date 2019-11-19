// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/instantapps/instant_apps_settings.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/android/chrome_jni_headers/InstantAppsSettings_jni.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/installable/installable_logging.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ConvertJavaStringToUTF8;

namespace {

// This histogram is used to record UMA, please do not rearrange,
// append entries only before AIA_COUNT.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.instantapps
// GENERATED_JAVA_PREFIX_TO_STRIP: AIA_
enum class AiaBannerReason {
  AIA_SHOULD_SHOW,
  AIA_ALREADY_INSTALLED,
  AIA_RECENTLY_BLOCKED,
  AIA_RECENTLY_IGNORED,
  AIA_IN_DOMAIN_NAVIGATION,
  AIA_COUNT
};

void RecordShouldShowBannerMetric(AiaBannerReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Android.InstantApps.ShouldShowBanner", reason,
                            AiaBannerReason::AIA_COUNT);
}

}  // namespace

void InstantAppsSettings::RecordInfoBarShowEvent(
    content::WebContents* web_contents,
    const std::string& url) {
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents,
      GURL(url),
      AppBannerSettingsHelper::kInstantAppsKey,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW,
      base::Time::Now());
}

void InstantAppsSettings::RecordInfoBarDismissEvent(
    content::WebContents* web_contents,
    const std::string& url) {
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents,
      GURL(url),
      AppBannerSettingsHelper::kInstantAppsKey,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK,
      base::Time::Now());
}

static void JNI_InstantAppsSettings_SetInstantAppDefault(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jurl) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  std::string url(ConvertJavaStringToUTF8(env, jurl));

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents,
      GURL(url),
      AppBannerSettingsHelper::kInstantAppsKey,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      base::Time::Now());
}

static jboolean JNI_InstantAppsSettings_GetInstantAppDefault(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jurl) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  std::string url(ConvertJavaStringToUTF8(env, jurl));

  base::Time added_time = AppBannerSettingsHelper::GetSingleBannerEvent(
      web_contents,
      GURL(url),
      AppBannerSettingsHelper::kInstantAppsKey,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN);

  return !added_time.is_null();
}

static jboolean JNI_InstantAppsSettings_ShouldShowBanner(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jurl) {
  content::WebContents* web_contents =
        content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  GURL url(ConvertJavaStringToUTF8(env, jurl));
  const std::string& key = AppBannerSettingsHelper::kInstantAppsKey;
  base::Time now = base::Time::Now();

  if (AppBannerSettingsHelper::HasBeenInstalled(web_contents, url, key)) {
    RecordShouldShowBannerMetric(AiaBannerReason::AIA_ALREADY_INSTALLED);
    return false;
  }

  if (AppBannerSettingsHelper::WasBannerRecentlyBlocked(web_contents, url, key,
                                                        now)) {
    RecordShouldShowBannerMetric(AiaBannerReason::AIA_RECENTLY_BLOCKED);
    return false;
  }

  if (AppBannerSettingsHelper::WasBannerRecentlyIgnored(web_contents, url, key,
                                                        now)) {
    RecordShouldShowBannerMetric(AiaBannerReason::AIA_RECENTLY_IGNORED);
    return false;
  }

  return true;
}
