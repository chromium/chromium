// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/external_prerender_handler_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/ExternalPrerenderHandler_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/web_contents.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;

namespace prerender {

namespace {

bool JNI_ExternalPrerenderHandler_CheckAndConvertParams(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jobject>& jweb_contents,
    GURL* url,
    PrerenderManager** prerender_manager,
    content::WebContents** web_contents) {
  if (!jurl)
    return false;

  *url = GURL(ConvertJavaStringToUTF16(env, jurl));
  if (!url->is_valid())
    return false;

  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  *prerender_manager = PrerenderManagerFactory::GetForBrowserContext(profile);
  if (!*prerender_manager)
    return false;

  *web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  return true;
}

}  // namespace

base::android::ScopedJavaLocalRef<jobject>
ExternalPrerenderHandlerAndroid::AddPrerender(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jstring>& jreferrer,
    jint top,
    jint left,
    jint bottom,
    jint right,
    jboolean forced_prerender) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);

  GURL url = GURL(ConvertJavaStringToUTF16(env, jurl));
  if (!url.is_valid())
    return nullptr;
  content::Referrer referrer;
  if (!jreferrer.is_null()) {
    GURL referrer_url(ConvertJavaStringToUTF16(env, jreferrer));
    if (referrer_url.is_valid()) {
      referrer = content::Referrer(referrer_url,
                                   network::mojom::ReferrerPolicy::kDefault);
    }
  }

  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(profile);
  if (!prerender_manager)
    return nullptr;

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (prerender_handle_)
    prerender_handle_->OnNavigateAway();

  if (forced_prerender) {
    prerender_handle_ =
        prerender_manager->AddForcedPrerenderFromExternalRequest(
            url, referrer,
            web_contents->GetController().GetDefaultSessionStorageNamespace(),
            gfx::Rect(left, top, right - left, bottom - top));
  } else {
    prerender_handle_ = prerender_manager->AddPrerenderFromExternalRequest(
        url, referrer,
        web_contents->GetController().GetDefaultSessionStorageNamespace(),
        gfx::Rect(left, top, right - left, bottom - top));
  }

  if (!prerender_handle_) {
    return nullptr;
  } else {
    return prerender_handle_
        ->contents()->prerender_contents()->GetJavaWebContents();
  }
}

void ExternalPrerenderHandlerAndroid::CancelCurrentPrerender(JNIEnv* env) {
  if (!prerender_handle_)
    return;

  prerender_handle_->OnCancel();
  prerender_handle_.reset();
}

static jboolean JNI_ExternalPrerenderHandler_HasPrerenderedUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jobject>& jweb_contents) {
  GURL url;
  PrerenderManager* prerender_manager;
  content::WebContents* web_contents;
  if (!JNI_ExternalPrerenderHandler_CheckAndConvertParams(
          env, jprofile, jurl, jweb_contents, &url, &prerender_manager,
          &web_contents))
    return false;

  return prerender_manager->HasPrerenderedUrl(url, web_contents);
}

static jboolean JNI_ExternalPrerenderHandler_HasRecentlyPrefetchedUrlForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jurl) {
  if (!jurl)
    return false;

  GURL url(ConvertJavaStringToUTF16(env, jurl));
  if (!url.is_valid())
    return false;

  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(profile);
  if (!prerender_manager)
    return false;
  return prerender_manager->HasRecentlyPrefetchedUrlForTesting(url);
}

static void JNI_ExternalPrerenderHandler_ClearPrefetchInformationForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(profile);
  if (!prerender_manager)
    return;
  prerender_manager->ClearPrefetchInformationForTesting();
}

ExternalPrerenderHandlerAndroid::ExternalPrerenderHandlerAndroid() {}

ExternalPrerenderHandlerAndroid::~ExternalPrerenderHandlerAndroid() {}

static jlong JNI_ExternalPrerenderHandler_Init(JNIEnv* env) {
  ExternalPrerenderHandlerAndroid* external_handler =
      new ExternalPrerenderHandlerAndroid();
  return reinterpret_cast<intptr_t>(external_handler);
}

}  // namespace prerender
