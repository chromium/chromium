// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_bridge.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "chrome/browser/android/tab_android.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// JNI headers must be included after standard headers to ensure types like
// ui::WindowAndroid and content::WebContents are declared before use, and to
// avoid 'specialization after instantiation' errors for ToJniType.
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/jni_headers/CoBrowseViewFactory_jni.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/jni_headers/CoBrowseViews_jni.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/jni_headers/TabBottomSheetNativeInterface_jni.h"

using base::android::AttachCurrentThread;

namespace context_sharing {

void JNI_TabBottomSheetNativeInterface_OnClosed(
    JNIEnv* env,
    int64_t native_tab_bottom_sheet_bridge) {
  reinterpret_cast<TabBottomSheetBridge*>(native_tab_bottom_sheet_bridge)
      ->OnClosed(env);
}

void JNI_TabBottomSheetNativeInterface_OnSuppressed(
    JNIEnv* env,
    int64_t native_tab_bottom_sheet_bridge) {
  reinterpret_cast<TabBottomSheetBridge*>(native_tab_bottom_sheet_bridge)
      ->OnSuppressed(env);
}

void JNI_TabBottomSheetNativeInterface_OnOpened(
    JNIEnv* env,
    int64_t native_tab_bottom_sheet_bridge,
    bool is_expanded) {
  reinterpret_cast<TabBottomSheetBridge*>(native_tab_bottom_sheet_bridge)
      ->OnOpened(env, is_expanded);
}

TabBottomSheetBridge::TabBottomSheetBridge(Observer* observer,
                                           tabs::TabInterface* tab,
                                           TabBottomSheetClientType client_type)
    : observer_(observer), tab_(*tab), client_type_(client_type) {
  JNIEnv* env = AttachCurrentThread();
  java_bridge_.Reset(Java_TabBottomSheetNativeInterface_Constructor(
      env, reinterpret_cast<intptr_t>(this), GetTabAndroid()->GetJavaObject()));

  CreateCoBrowseViews(/*web_contents=*/nullptr);
}

TabBottomSheetBridge::~TabBottomSheetBridge() {
  DestroyCoBrowseViews();
  if (java_bridge_) {
    Java_TabBottomSheetNativeInterface_destroy(AttachCurrentThread(),
                                               java_bridge_);
  }
}

void TabBottomSheetBridge::SetWebContents(content::WebContents* web_contents) {
  if (web_contents) {
    web_contents->SetIgnoreZoomGestures(true);
  }

  TabAndroid* tab_android = GetTabAndroid();
  content::WebContents* tab_contents =
      tab_android ? tab_android->GetContents() : nullptr;
  ui::WindowAndroid* current_window =
      (tab_contents && !tab_android->IsOffscreenRendering())
          ? tab_contents->GetTopLevelNativeWindow()
          : nullptr;

  if (tab_contents && tab_android->IsOffscreenRendering()) {
    LOG(WARNING)
        << "Tab is offscreen rendering, current_window is set to null.";
  }

  if (!co_browse_views_ || (current_window != window_android_ &&
                            !tab_android->IsOffscreenRendering())) {
    CreateCoBrowseViews(web_contents);
    return;
  }

  Java_CoBrowseViews_setWebContents(AttachCurrentThread(), co_browse_views_,
                                    web_contents);
}

bool TabBottomSheetBridge::Show(bool animate, bool starts_expanded) {
  if (!co_browse_views_) {
    return false;
  }
  return Java_TabBottomSheetNativeInterface_show(AttachCurrentThread(),
                                                 java_bridge_, co_browse_views_,
                                                 animate, starts_expanded);
}

void TabBottomSheetBridge::Close(bool animate) {
  if (co_browse_views_) {
    SetWebContents(nullptr);
  }
  Java_TabBottomSheetNativeInterface_close(AttachCurrentThread(), java_bridge_,
                                           animate);
}

void TabBottomSheetBridge::OnClosed(JNIEnv* env) {
  observer_->OnClosed();
}

void TabBottomSheetBridge::OnSuppressed(JNIEnv* env) {
  observer_->OnSuppressed();
}

void TabBottomSheetBridge::OnOpened(JNIEnv* env, bool is_expanded) {
  observer_->OnOpened(is_expanded);
}

void TabBottomSheetBridge::CreateCoBrowseViews(
    content::WebContents* web_contents) {
  TabAndroid* tab_android = GetTabAndroid();
  if (!tab_android) {
    VLOG(1) << "Cannot create CoBrowseViews: TabAndroid is null.";
    return;
  }

  content::WebContents* tab_contents = tab_android->GetContents();
  if (!tab_contents) {
    VLOG(1) << "Cannot create CoBrowseViews: TabAndroid has no WebContents.";
    return;
  }

  ui::WindowAndroid* window_android = tab_contents->GetTopLevelNativeWindow();
  if (!window_android) {
    VLOG(1) << "Cannot create CoBrowseViews: WindowAndroid is null.";
    return;
  }

  DestroyCoBrowseViews();

  window_android_ = window_android;

  JNIEnv* env = base::android::AttachCurrentThread();
  // Call Factory to get CoBrowseViews and save it
  co_browse_views_.Reset(Java_CoBrowseViewFactory_buildCoBrowseViews(
      env, window_android, web_contents, static_cast<int>(client_type_)));
}

void TabBottomSheetBridge::DestroyCoBrowseViews() {
  if (!co_browse_views_) {
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  Java_CoBrowseViews_setWebContents(env, co_browse_views_,
                                    /*webContents=*/nullptr);
  Java_CoBrowseViews_destroy(env, co_browse_views_);
  co_browse_views_.Reset();
  window_android_ = nullptr;
}

TabAndroid* TabBottomSheetBridge::GetTabAndroid() const {
  return TabAndroid::FromTabHandle(tab_->GetHandle());
}

DEFINE_JNI(TabBottomSheetNativeInterface)

}  // namespace context_sharing
