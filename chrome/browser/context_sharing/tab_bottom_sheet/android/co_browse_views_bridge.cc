// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_views_bridge.h"

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_container_type.h"
#include "components/tabs/public/tab_interface.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// JNI headers must be included after standard headers to ensure types like
// ui::WindowAndroid and content::WebContents are declared before use, and to
// avoid 'specialization after instantiation' errors for ToJniType.
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/jni_headers/CoBrowseViewFactory_jni.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/jni_headers/CoBrowseViews_jni.h"

using base::android::AttachCurrentThread;

namespace context_sharing {

// static
base::android::ScopedJavaLocalRef<jobject>
CoBrowseViewsBridge::GetViewFromCoBrowseViews(
    const base::android::JavaRef<jobject>& java_co_browse_views) {
  if (!java_co_browse_views) {
    return nullptr;
  }
  JNIEnv* env = AttachCurrentThread();
  return Java_CoBrowseViews_getView(env, java_co_browse_views);
}

CoBrowseViewsBridge::CoBrowseViewsBridge(
    tabs::TabInterface& tab,
    context_sharing::TabBottomSheetClientType client_type,
    context_sharing::CoBrowseContainerType container_type,
    const base::android::JavaRef<jobject>& bottom_sheet_content_provider)
    : tab_(tab),
      client_type_(client_type),
      container_type_(container_type),
      bottom_sheet_content_provider_(bottom_sheet_content_provider) {}

CoBrowseViewsBridge::~CoBrowseViewsBridge() {
  DestroyCoBrowseViews();
}

bool CoBrowseViewsBridge::CreateCoBrowseViews(
    content::WebContents* web_contents) {
  TabAndroid* tab_android = GetTabAndroid();
  if (!tab_android) {
    LOG(WARNING) << "Cannot create CoBrowseViews: TabAndroid is null.";
    return false;
  }

  content::WebContents* tab_contents = tab_android->GetContents();
  if (!tab_contents) {
    LOG(WARNING)
        << "Cannot create CoBrowseViews: TabAndroid has no WebContents.";
    return false;
  }

  ui::WindowAndroid* window_android = tab_contents->GetTopLevelNativeWindow();
  if (!window_android) {
    LOG(WARNING) << "Cannot create CoBrowseViews: WindowAndroid is null.";
    return false;
  }

  DestroyCoBrowseViews();

  window_android_ = window_android;

  JNIEnv* env = AttachCurrentThread();
  java_co_browse_views_.Reset(Java_CoBrowseViewFactory_buildCoBrowseViews(
      env, window_android, web_contents, static_cast<int>(client_type_),
      static_cast<int>(container_type_), bottom_sheet_content_provider_));

  return !java_co_browse_views_.is_null();
}

void CoBrowseViewsBridge::SetWebContents(content::WebContents* web_contents,
                                         bool request_focus) {
  if (web_contents) {
    web_contents->SetIgnoreZoomGestures(true);
    if (!zoom::ZoomController::FromWebContents(web_contents)) {
      zoom::ZoomController::CreateForWebContents(web_contents);
    }
  }

  TabAndroid* tab_android = GetTabAndroid();
  CHECK(tab_android);
  content::WebContents* tab_contents = tab_android->GetContents();
  ui::WindowAndroid* current_window =
      (tab_contents && !tab_android->IsOffscreenRendering())
          ? tab_contents->GetTopLevelNativeWindow()
          : nullptr;

  if (tab_contents && tab_android->IsOffscreenRendering()) {
    LOG(WARNING)
        << "Tab is offscreen rendering, current_window is set to null.";
  }

  if (!java_co_browse_views_ || (current_window != window_android_ &&
                                 !tab_android->IsOffscreenRendering())) {
    CreateCoBrowseViews(web_contents);
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Java_CoBrowseViews_setWebContents(env, java_co_browse_views_, web_contents,
                                    request_focus);
}

base::android::ScopedJavaLocalRef<jobject>
CoBrowseViewsBridge::GetCoBrowseViews() {
  if (!java_co_browse_views_) {
    return nullptr;
  }
  JNIEnv* env = AttachCurrentThread();
  return base::android::ScopedJavaLocalRef<jobject>(env, java_co_browse_views_);
}

void CoBrowseViewsBridge::DestroyCoBrowseViews() {
  if (!java_co_browse_views_) {
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  Java_CoBrowseViews_setWebContents(env, java_co_browse_views_, nullptr,
                                    /*request_focus=*/false);
  Java_CoBrowseViews_destroy(env, java_co_browse_views_);
  java_co_browse_views_.Reset();
  window_android_ = nullptr;
}

TabAndroid* CoBrowseViewsBridge::GetTabAndroid() const {
  return TabAndroid::FromTabHandle(tab_->GetHandle());
}

}  // namespace context_sharing
