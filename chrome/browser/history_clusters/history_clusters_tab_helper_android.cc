// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"

#include "base/android/jni_android.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/history_clusters/jni_headers/HistoryClustersTabHelper_jni.h"

static void JNI_HistoryClustersTabHelper_OnCurrentTabUrlCopied(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  if (auto* clusters_helper = HistoryClustersTabHelper::FromWebContents(
          content::WebContents::FromJavaWebContents(j_web_contents))) {
    clusters_helper->OnOmniboxUrlCopied();
  }
}

static void JNI_HistoryClustersTabHelper_OnCurrentTabUrlShared(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  if (auto* clusters_helper = HistoryClustersTabHelper::FromWebContents(
          content::WebContents::FromJavaWebContents(j_web_contents))) {
    clusters_helper->OnOmniboxUrlShared();
  }
}
