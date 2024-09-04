// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/CustomTabAuthUrlHeuristics_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/password_manager/android/first_cct_page_load_marker.h"

using base::android::JavaParamRef;

static void JNI_CustomTabAuthUrlHeuristics_SetFirstCctPageLoadForPasswords(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);

  if (!tab || !tab->web_contents()) {
    return;
  }

  FirstCctPageLoadMarker::CreateForWebContents(tab->web_contents());
}
