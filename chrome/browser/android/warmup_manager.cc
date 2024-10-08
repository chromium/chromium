// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/preloading/prefetch/chrome_prefetch_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WarmupManager_jni.h"

using base::android::JavaParamRef;

static void JNI_WarmupManager_StartPreconnectPredictorInitialization(
    JNIEnv* env,
    Profile* profile) {
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (!loading_predictor)
    return;
  loading_predictor->StartInitialization();
}

static void JNI_WarmupManager_PreconnectUrlAndSubresources(
    JNIEnv* env,
    Profile* profile,
    std::string& url_str) {
  GURL url = GURL(url_str);

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (loading_predictor) {
    loading_predictor->PrepareForPageLoad(/*initiator_origin=*/std::nullopt,
                                          url,
                                          predictors::HintOrigin::EXTERNAL);
  }
}

static void JNI_WarmupManager_StartPrefetchFromCCT(
    JNIEnv* env,
    content::WebContents* web_contents,
    GURL& url,
    jboolean juse_prefetch_proxy,
    std::optional<url::Origin>& trusted_source_origin) {
  ChromePrefetchManager::GetOrCreateForWebContents(web_contents)
      ->StartPrefetchFromCCT(url, juse_prefetch_proxy, trusted_source_origin);
}
