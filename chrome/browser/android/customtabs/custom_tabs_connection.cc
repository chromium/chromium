// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/android/customtabs/client_data_header_web_contents_observer.h"
#include "chrome/browser/android/customtabs/detached_resource_request.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/common/referrer.h"
#include "net/url_request/referrer_policy.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/CustomTabsConnection_jni.h"

namespace customtabs {

namespace {

void NotifyClientOfDetachedRequestCompletion(
    const base::android::ScopedJavaGlobalRef<jobject>& session,
    const GURL& url,
    int net_error) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CustomTabsConnection_notifyClientOfDetachedRequestCompletion(
      env, session, url.spec(), net_error);
}

}  // namespace

static void JNI_CustomTabsConnection_CreateAndStartDetachedResourceRequest(
    JNIEnv* env,
    Profile* native_profile,
    const base::android::JavaRef<jobject>& session,
    const std::string& package_name,
    const std::string& url,
    const std::string& origin,
    int32_t referrer_policy,
    int32_t motivation) {
  DCHECK(native_profile);

  GURL native_url(url);
  GURL native_origin(origin);
  DCHECK(native_url.is_valid());
  DCHECK(native_origin.is_valid());

  // Java only knows about the blink referrer policy.
  net::ReferrerPolicy url_request_referrer_policy =
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
      request_motivation, package_name, std::move(cb));
}

static void JNI_CustomTabsConnection_SetClientDataHeader(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents,
    const std::string& jheader) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  ClientDataHeaderWebContentsObserver::CreateForWebContents(web_contents);
  ClientDataHeaderWebContentsObserver::FromWebContents(web_contents)
      ->SetHeader(jheader);
}

}  // namespace customtabs

DEFINE_JNI(CustomTabsConnection)
