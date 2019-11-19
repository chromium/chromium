// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/WarmupManager_jni.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/render_process_host.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

static void JNI_WarmupManager_StartPreconnectPredictorInitialization(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (!loading_predictor)
    return;
  loading_predictor->StartInitialization();
}

static void JNI_WarmupManager_PreconnectUrlAndSubresources(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& url_str) {
  if (url_str) {
    GURL url = GURL(base::android::ConvertJavaStringToUTF8(env, url_str));
    Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);

    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(profile);
    if (loading_predictor) {
      loading_predictor->PrepareForPageLoad(url,
                                            predictors::HintOrigin::EXTERNAL);
    }
  }
}

static void JNI_WarmupManager_WarmupSpareRenderer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (profile) {
    content::RenderProcessHost::WarmupSpareRenderProcessHost(profile);
  }
}
