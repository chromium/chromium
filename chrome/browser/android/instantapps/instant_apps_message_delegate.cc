// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/instantapps/instant_apps_message_delegate.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/user_metrics.h"
#include "chrome/android/chrome_jni_headers/InstantAppsMessageDelegate_jni.h"
#include "chrome/browser/android/instantapps/instant_apps_settings.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
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

bool ShouldDismissMessage(const content::LoadCommittedDetails& load_details) {
  const ui::PageTransition transition = load_details.entry->GetTransitionType();
  return load_details.is_navigation_to_different_page() &&
         !load_details.did_replace_entry &&
         (load_details.previous_entry_index !=
              load_details.entry->GetUniqueID() ||
          ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD));
}

}  // namespace

InstantAppsMessageDelegate::~InstantAppsMessageDelegate() {}

InstantAppsMessageDelegate::InstantAppsMessageDelegate(
    content::WebContents* web_contents,
    const jobject jdelegate,
    const std::string& url)
    : content::WebContentsObserver(web_contents),
      url_(url),
      user_navigated_away_from_launch_url_(false) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(env, jdelegate);
}

void InstantAppsMessageDelegate::PrimaryPageChanged(content::Page& page) {
  if (page.GetMainDocument().IsErrorDocument()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InstantAppsMessageDelegate_dismissMessage(env, java_delegate_);
  }
}

// TODO(crbug.com/1296762): Starting a navigation does not guarantee that it
// will be committed and user_navigated_away_from_launch_url_ may be incorrectly
// updated.
void InstantAppsMessageDelegate::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;
  if (!user_navigated_away_from_launch_url_ &&
      !GURL(url_).EqualsIgnoringRef(
          navigation_handle->GetWebContents()->GetLastCommittedURL())) {
    user_navigated_away_from_launch_url_ =
        PageTransitionInitiatedByUser(navigation_handle);
  }
}

void InstantAppsMessageDelegate::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (user_navigated_away_from_launch_url_ &&
      ShouldDismissMessage(load_details)) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InstantAppsMessageDelegate_dismissMessage(env, java_delegate_);
  }
}

jlong JNI_InstantAppsMessageDelegate_InitializeNativeDelegate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jdelegate,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jurl) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  std::string url(base::android::ConvertJavaStringToUTF8(env, jurl));
  return reinterpret_cast<intptr_t>(
      new InstantAppsMessageDelegate(web_contents, jdelegate, url));
}

void JNI_InstantAppsMessageDelegate_OnMessageShown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jurl,
    const jboolean instant_app_is_default) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  std::string url(base::android::ConvertJavaStringToUTF8(env, jurl));
  InstantAppsSettings::RecordShowEvent(web_contents, url);
  if (instant_app_is_default) {
    base::RecordAction(
        base::UserMetricsAction("Android.InstantApps.BannerShownAppIsDefault"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Android.InstantApps.BannerShown"));
  }
}

void JNI_InstantAppsMessageDelegate_OnPrimaryAction(
    JNIEnv* env,
    const jboolean instant_app_is_default) {
  if (instant_app_is_default) {
    base::RecordAction(
        base::UserMetricsAction("Android.InstantApps.BannerOpenAppIsDefault"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Android.InstantApps.BannerOpen"));
  }
}

void JNI_InstantAppsMessageDelegate_OnMessageDismissed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jurl,
    const jboolean instant_app_is_default) {
  if (instant_app_is_default) {
    base::RecordAction(base::UserMetricsAction(
        "Android.InstantApps.BannerDismissedAppIsDefault"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Android.InstantApps.BannerDismissed"));
  }
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return;
  std::string url(base::android::ConvertJavaStringToUTF8(env, jurl));
  InstantAppsSettings::RecordDismissEvent(web_contents, url);
}
