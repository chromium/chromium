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
                                           tabs::TabInterface* tab)
    : observer_(observer), tab_(*tab) {
  JNIEnv* env = AttachCurrentThread();
  java_bridge_.Reset(Java_TabBottomSheetNativeInterface_Constructor(
      env, reinterpret_cast<intptr_t>(this), GetTabAndroid()->GetJavaObject()));
}

TabBottomSheetBridge::~TabBottomSheetBridge() {
  if (java_bridge_) {
    Java_TabBottomSheetNativeInterface_destroy(AttachCurrentThread(),
                                               java_bridge_);
  }
}

bool TabBottomSheetBridge::Show(
    const base::android::JavaRef<jobject>& co_browse_views,
    bool animate,
    bool starts_expanded) {
  if (co_browse_views.is_null()) {
    return false;
  }
  return Java_TabBottomSheetNativeInterface_show(AttachCurrentThread(),
                                                 java_bridge_, co_browse_views,
                                                 animate, starts_expanded);
}

void TabBottomSheetBridge::Close(bool animate) {
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

TabAndroid* TabBottomSheetBridge::GetTabAndroid() const {
  return TabAndroid::FromTabHandle(tab_->GetHandle());
}

DEFINE_JNI(TabBottomSheetNativeInterface)

}  // namespace context_sharing
