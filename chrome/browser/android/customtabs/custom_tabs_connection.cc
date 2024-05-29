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
#include "chrome/browser/android/customtabs/text_fragment_lookup_state_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/common/referrer.h"
#include "net/url_request/referrer_policy.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/CustomTabsConnection_jni.h"

namespace customtabs {

namespace {

std::vector<std::string> ConvertJavaStringArrayToListValue(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& array) {
  DCHECK(!array.is_null());
  base::android::JavaObjectArrayReader<jstring> array_reader(array);
  DCHECK_GE(array_reader.size(), 0)
      << "Invalid array length: " << array_reader.size();

  std::vector<std::string> vector_string;
  for (auto j_str : array_reader) {
    vector_string.push_back(base::android::ConvertJavaStringToUTF8(env, j_str));
  }

  return vector_string;
}

void NotifyClientOfDetachedRequestCompletion(
    const base::android::ScopedJavaGlobalRef<jobject>& session,
    const GURL& url,
    int net_error) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CustomTabsConnection_notifyClientOfDetachedRequestCompletion(
      env, session, base::android::ConvertUTF8ToJavaString(env, url.spec()),
      net_error);
}

// Notify client of text fragment look up completion.
// `state_key` is opaque id used by client to keep track of the request.
// `lookup_results` is the mapping between the text fragments that were looked
// up and whether they were found on the page or not.
void NotifyClientOfTextFragmentLookupCompletion(
    const base::android::ScopedJavaGlobalRef<jobject>& session,
    const std::string& state_key,
    const std::vector<std::pair<std::string, bool>>& lookup_results) {
  std::vector<std::string> found_fragments;

  // Extract the fragments that were found on the page.
  for (auto it : lookup_results) {
    if (it.second) {
      found_fragments.push_back(it.first);
    }
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CustomTabsConnection_notifyClientOfTextFragmentLookupCompletion(
      env, session, base::android::ConvertUTF8ToJavaString(env, state_key),
      base::android::ToJavaArrayOfStrings(env, found_fragments));
}

}  // namespace

static void JNI_CustomTabsConnection_CreateAndStartDetachedResourceRequest(
    JNIEnv* env,
    Profile* native_profile,
    const base::android::JavaParamRef<jobject>& session,
    const base::android::JavaParamRef<jstring>& package_name,
    const base::android::JavaParamRef<jstring>& url,
    const base::android::JavaParamRef<jstring>& origin,
    jint referrer_policy,
    jint motivation) {
  DCHECK(native_profile);
  DCHECK(url);
  DCHECK(origin);

  GURL native_url(base::android::ConvertJavaStringToUTF8(env, url));
  GURL native_origin(base::android::ConvertJavaStringToUTF8(env, origin));
  DCHECK(native_url.is_valid());
  DCHECK(native_origin.is_valid());

  std::string native_package;
  if (!package_name.is_null())
    base::android::ConvertJavaStringToUTF8(env, package_name, &native_package);

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
      request_motivation, native_package, std::move(cb));
}

static void JNI_CustomTabsConnection_SetClientDataHeader(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jheader) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  ClientDataHeaderWebContentsObserver::CreateForWebContents(web_contents);
  ClientDataHeaderWebContentsObserver::FromWebContents(web_contents)
      ->SetHeader(base::android::ConvertJavaStringToUTF8(jheader));
}

static void JNI_CustomTabsConnection_TextFragmentLookup(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& session,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jstate_key,
    const base::android::JavaParamRef<jobjectArray>& jtext_fragments) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);

  TextFragmentLookupStateTracker::OnResultCallback cb =
      session.is_null()
          ? base::DoNothing()
          : base::BindOnce(
                &NotifyClientOfTextFragmentLookupCompletion,
                base::android::ScopedJavaGlobalRef<jobject>(session));

  TextFragmentLookupStateTracker::CreateForWebContents(web_contents);
  TextFragmentLookupStateTracker::FromWebContents(web_contents)
      ->LookupTextFragment(
          base::android::ConvertJavaStringToUTF8(jstate_key),
          ConvertJavaStringArrayToListValue(env, jtext_fragments),
          std::move(cb));
}

static void JNI_CustomTabsConnection_TextFragmentFindScrollAndHighlight(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& session,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jtext_fragment) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);

  TextFragmentLookupStateTracker::CreateForWebContents(web_contents);
  TextFragmentLookupStateTracker::FromWebContents(web_contents)
      ->FindScrollAndHighlight(
          base::android::ConvertJavaStringToUTF8(jtext_fragment));
}

}  // namespace customtabs
