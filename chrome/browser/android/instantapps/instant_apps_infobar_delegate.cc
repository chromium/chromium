// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/instantapps/instant_apps_infobar_delegate.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/user_metrics.h"
#include "chrome/android/chrome_jni_headers/InstantAppsInfoBarDelegate_jni.h"
#include "chrome/browser/android/instantapps/instant_apps_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/instant_apps_infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace {

bool PageTransitionInitiatedByUser(
    content::NavigationHandle* navigation_handle) {
  auto page_transition = navigation_handle->GetPageTransition();
  return navigation_handle->HasUserGesture() ||
         (page_transition & ui::PAGE_TRANSITION_FORWARD_BACK) ||
         (page_transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) ||
         (page_transition & ui::PAGE_TRANSITION_HOME_PAGE) ||
         ui::PageTransitionCoreTypeIs(page_transition,
                                      ui::PAGE_TRANSITION_TYPED);
}

}  // namespace

InstantAppsInfoBarDelegate::~InstantAppsInfoBarDelegate() {}

// static
void InstantAppsInfoBarDelegate::Create(content::WebContents* web_contents,
                                        const jobject jdata,
                                        const std::string& url,
                                        bool instant_app_is_default) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(std::make_unique<InstantAppsInfoBar>(
      std::unique_ptr<InstantAppsInfoBarDelegate>(
          new InstantAppsInfoBarDelegate(web_contents, jdata, url,
                                         instant_app_is_default))));
}

InstantAppsInfoBarDelegate::InstantAppsInfoBarDelegate(
    content::WebContents* web_contents,
    const jobject jdata,
    const std::string& url,
    bool instant_app_is_default)
    : content::WebContentsObserver(web_contents),
      url_(url),
      user_navigated_away_from_launch_url_(false),
      instant_app_is_default_(instant_app_is_default) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(Java_InstantAppsInfoBarDelegate_create(env));
  data_.Reset(env, jdata);
}

infobars::InfoBarDelegate::InfoBarIdentifier
InstantAppsInfoBarDelegate::GetIdentifier() const {
  return INSTANT_APPS_INFOBAR_DELEGATE_ANDROID;
}

base::string16 InstantAppsInfoBarDelegate::GetMessageText() const {
  // Message is set in InstantAppInfobar.java
  return base::string16();
}

bool InstantAppsInfoBarDelegate::Accept() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (instant_app_is_default_) {
    base::RecordAction(
        base::UserMetricsAction("Android.InstantApps.BannerOpenAppIsDefault"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Android.InstantApps.BannerOpen"));
  }
  Java_InstantAppsInfoBarDelegate_openInstantApp(env, java_delegate_, data_);
  return true;
}

bool InstantAppsInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate && delegate->GetIdentifier() == GetIdentifier();
}


void InstantAppsInfoBarDelegate::InfoBarDismissed() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  InstantAppsSettings::RecordInfoBarDismissEvent(web_contents, url_);
  if (instant_app_is_default_) {
    base::RecordAction(base::UserMetricsAction(
        "Android.InstantApps.BannerDismissedAppIsDefault"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Android.InstantApps.BannerDismissed"));
  }
}

void InstantAppsInfoBarDelegate::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!user_navigated_away_from_launch_url_ &&
      !GURL(url_).EqualsIgnoringRef(
          navigation_handle->GetWebContents()->GetURL())) {
    user_navigated_away_from_launch_url_ =
        PageTransitionInitiatedByUser(navigation_handle);
  }
}

void InstantAppsInfoBarDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsErrorPage()) {
    infobar()->RemoveSelf();
  }
}

bool InstantAppsInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return user_navigated_away_from_launch_url_ &&
         ConfirmInfoBarDelegate::ShouldExpire(details);
}

void JNI_InstantAppsInfoBarDelegate_Launch(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jobject>& jdata,
    const base::android::JavaParamRef<jstring>& jurl,
    const jboolean instant_app_is_default) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  std::string url(base::android::ConvertJavaStringToUTF8(env, jurl));

  InstantAppsInfoBarDelegate::Create(web_contents, jdata, url,
                                     instant_app_is_default);
  InstantAppsSettings::RecordInfoBarShowEvent(web_contents, url);

  if (instant_app_is_default) {
    base::RecordAction(
        base::UserMetricsAction("Android.InstantApps.BannerShownAppIsDefault"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Android.InstantApps.BannerShown"));
  }
}
