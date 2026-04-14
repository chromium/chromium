// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/co_browse_views_bridge.h"

#include "base/android/jni_android.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// JNI headers must be included after standard headers to ensure types like
// ui::WindowAndroid and content::WebContents are declared before use, and to
// avoid 'specialization after instantiation' errors for ToJniType.
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/jni_headers/CoBrowseViewFactory_jni.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/jni_headers/CoBrowseViews_jni.h"

using base::android::AttachCurrentThread;

namespace glic {

CoBrowseViewsBridge::CoBrowseViewsBridge(tabs::TabInterface& tab) : tab_(tab) {}

CoBrowseViewsBridge::~CoBrowseViewsBridge() {
  if (java_co_browse_views_) {
    JNIEnv* env = AttachCurrentThread();
    // Reset contents and destroy to prevent leaks
    Java_CoBrowseViews_setWebContents(env, java_co_browse_views_, nullptr);
    Java_CoBrowseViews_destroy(env, java_co_browse_views_);
    java_co_browse_views_.Reset();
  }
}

bool CoBrowseViewsBridge::CreateCoBrowseViews(
    content::WebContents* web_contents) {
  // Use the tab's WebContents to get the window.
  content::WebContents* tab_contents = tab_->GetContents();
  if (!tab_contents) {
    return false;
  }

  ui::WindowAndroid* window_android = tab_contents->GetTopLevelNativeWindow();
  if (!window_android) {
    return false;
  }

  JNIEnv* env = AttachCurrentThread();
  java_co_browse_views_.Reset(Java_CoBrowseViewFactory_buildCoBrowseViews(
      env, window_android, web_contents, /*show_toolbar=*/false,
      /*show_fusebox=*/false));

  return !java_co_browse_views_.is_null();
}

void CoBrowseViewsBridge::SetWebContents(content::WebContents* web_contents) {
  if (!java_co_browse_views_) {
    CreateCoBrowseViews(web_contents);
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  Java_CoBrowseViews_setWebContents(env, java_co_browse_views_, web_contents);
}

base::android::ScopedJavaLocalRef<jobject> CoBrowseViewsBridge::GetView() {
  if (!java_co_browse_views_) {
    return nullptr;
  }
  JNIEnv* env = AttachCurrentThread();
  auto view = Java_CoBrowseViews_getView(env, java_co_browse_views_);
  return view;
}

}  // namespace glic
