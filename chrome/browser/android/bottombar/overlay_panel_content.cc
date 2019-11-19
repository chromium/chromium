// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bottombar/overlay_panel_content.h"

#include <set>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/OverlayPanelContent_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "components/history/core/browser/history_service.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_controls_state.h"
#include "net/url_request/url_fetcher_impl.h"
#include "ui/android/view_android.h"

using base::android::JavaParamRef;

namespace {

const int kHistoryDeletionWindowSeconds = 2;

// Because we need a callback, this needs to exist.
void OnHistoryDeletionDone() {
}

}  // namespace

// This class manages the native behavior of the panel.
// Instances of this class are owned by the Java OverlayPanelContentl.
OverlayPanelContent::OverlayPanelContent(JNIEnv* env, jobject obj) {
  java_manager_.Reset(env, obj);
}

OverlayPanelContent::~OverlayPanelContent() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OverlayPanelContent_clearNativePanelContentPtr(env, java_manager_);
}

void OverlayPanelContent::Destroy(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  delete this;
}

void OverlayPanelContent::OnPhysicalBackingSizeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    jint width,
    jint height) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  gfx::Size size(width, height);
  web_contents->GetNativeView()->OnPhysicalBackingSizeChanged(size);
  web_contents->GetNativeView()->OnSizeChanged(width, height);
}

void OverlayPanelContent::RemoveLastHistoryEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& search_url,
    jlong search_start_time_ms) {
  // The deletion window is from the time a search URL was put in history, up
  // to a short amount of time later.
  base::Time begin_time = base::Time::FromJsTime(search_start_time_ms);
  base::Time end_time = begin_time +
      base::TimeDelta::FromSeconds(kHistoryDeletionWindowSeconds);

  history::HistoryService* service = HistoryServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile(),
      ServiceAccessType::EXPLICIT_ACCESS);
  if (service) {
    // NOTE(mathp): We are only removing |search_url| from the local history
    // because search results that are not promoted to a Tab do not make it to
    // the web history, only local.
    std::set<GURL> restrict_set;
    restrict_set.insert(
        GURL(base::android::ConvertJavaStringToUTF8(env, search_url)));
    service->ExpireHistoryBetween(restrict_set, begin_time, end_time,
                                  /*user_initiated*/ false,
                                  base::BindOnce(&OnHistoryDeletionDone),
                                  &history_task_tracker_);
  }
}

void OverlayPanelContent::SetWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jweb_contents_delegate) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  DCHECK(web_contents);

  // NOTE(pedrosimonetti): Takes ownership of the WebContents. This is to make
  // sure that the WebContens and the Compositor are in the same process.
  // TODO(pedrosimonetti): Confirm with dtrainor@ if the comment above
  // is accurate.
  web_contents_.reset(web_contents);

  web_contents_->SetIsOverlayContent(true);
  // TODO(pedrosimonetti): confirm if we need this after promoting it
  // to a real tab.
  TabAndroid::AttachTabHelpers(web_contents_.get());
  web_contents_delegate_.reset(
      new web_contents_delegate_android::WebContentsDelegateAndroid(
          env, jweb_contents_delegate));
  web_contents_->SetDelegate(web_contents_delegate_.get());
  ViewAndroidHelper::FromWebContents(web_contents_.get())
      ->SetViewAndroid(web_contents_->GetNativeView());
}

void OverlayPanelContent::DestroyWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  DCHECK(web_contents_.get());
  web_contents_.reset();
  // |web_contents_delegate_| may already be NULL at this point.
  web_contents_delegate_.reset();
}

void OverlayPanelContent::SetInterceptNavigationDelegate(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& delegate,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  navigation_interception::InterceptNavigationDelegate::Associate(
      web_contents,
      std::make_unique<navigation_interception::InterceptNavigationDelegate>(
          env, delegate));
}

void OverlayPanelContent::UpdateBrowserControlsState(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean are_controls_hidden) {
  if (!web_contents_)
    return;

  content::BrowserControlsState state = content::BROWSER_CONTROLS_STATE_SHOWN;
  if (are_controls_hidden)
    state = content::BROWSER_CONTROLS_STATE_HIDDEN;

  web_contents_->GetMainFrame()->UpdateBrowserControlsState(
      state, content::BROWSER_CONTROLS_STATE_BOTH, false);
}

jlong JNI_OverlayPanelContent_Init(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  OverlayPanelContent* content = new OverlayPanelContent(env, obj);
  return reinterpret_cast<intptr_t>(content);
}
