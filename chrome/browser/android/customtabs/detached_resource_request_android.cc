// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/android/chrome_jni_headers/CustomTabsConnection_jni.h"
#include "chrome/browser/android/customtabs/detached_resource_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace customtabs {

namespace {

void NotifyClientOfDetachedRequestCompletion(
    const base::android::ScopedJavaGlobalRef<jobject>& session,
    const GURL& url,
    int net_error) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CustomTabsConnection_notifyClientOfDetachedRequestCompletion(
      env, session, base::android::ConvertUTF8ToJavaString(env, url.spec()),
      net_error);
}

}  // namespace

static void JNI_CustomTabsConnection_CreateAndStartDetachedResourceRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profile,
    const base::android::JavaParamRef<jobject>& session,
    const base::android::JavaParamRef<jstring>& url,
    const base::android::JavaParamRef<jstring>& origin,
    jint referrer_policy,
    jint motivation) {
  DCHECK(profile && url && origin);

  Profile* native_profile = ProfileAndroid::FromProfileAndroid(profile);
  DCHECK(native_profile);

  GURL native_url(base::android::ConvertJavaStringToUTF8(env, url));
  GURL native_origin(base::android::ConvertJavaStringToUTF8(env, origin));
  DCHECK(native_url.is_valid());
  DCHECK(native_origin.is_valid());

  // Java only knows about the blink referrer policy.
  net::URLRequest::ReferrerPolicy url_request_referrer_policy =
      content::Referrer::ReferrerPolicyForUrlRequest(
          content::Referrer::ConvertToPolicy(referrer_policy));
  DetachedResourceRequest::Motivation request_motivation =
      static_cast<DetachedResourceRequest::Motivation>(motivation);

  DetachedResourceRequest::OnResultCallback cb =
      session.is_null()
          ? base::DoNothing()
          : base::BindOnce(&NotifyClientOfDetachedRequestCompletion,
                           base::android::ScopedJavaGlobalRef<jobject>(session),
                           native_url);

  DetachedResourceRequest::CreateAndStart(
      native_profile, native_url, native_origin, url_request_referrer_policy,
      request_motivation, std::move(cb));
}

}  // namespace customtabs
