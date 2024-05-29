// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "content/public/browser/navigation_ui_data.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeNavigationUIData_jni.h"

static jlong JNI_ChromeNavigationUIData_CreateUnownedNativeCopy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong bookmark_id) {
  ChromeNavigationUIData* ui_data = new ChromeNavigationUIData();
  ui_data->set_bookmark_id(bookmark_id);
  return reinterpret_cast<intptr_t>(
      static_cast<content::NavigationUIData*>(ui_data));
}
