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
    const JavaParamRef<jstring>& url_str) {
  if (url_str) {
    GURL url = GURL(base::android::ConvertJavaStringToUTF8(env, url_str));

    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(profile);
    if (loading_predictor) {
      loading_predictor->PrepareForPageLoad(/*initiator_origin=*/std::nullopt,
                                            url,
                                            predictors::HintOrigin::EXTERNAL);
    }
  }
}

static void JNI_WarmupManager_StartPrefetchFromCCT(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jobject>& jurl,
    jboolean juse_prefetch_proxy,
    const base::android::JavaParamRef<jobject>& jtrusted_source_origin) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents);

  std::optional<url::Origin> trusted_source_origin = std::nullopt;
  if (jtrusted_source_origin != nullptr) {
    trusted_source_origin = url::Origin::FromJavaObject(jtrusted_source_origin);
  }

  return ChromePrefetchManager::GetOrCreateForWebContents(web_contents)
      ->StartPrefetchFromCCT(url::GURLAndroid::ToNativeGURL(env, jurl),
                             juse_prefetch_proxy, trusted_source_origin);
}
