// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/android/cookies/cookies_fetcher_restore_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/android/cookies/jni_headers/CookiesFetcher_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Passes the fetched |cookies| to the application so that can be saved in a
// file.
void OnCookiesFetchFinished(
    const ScopedJavaGlobalRef<jobject>& j_cookie_fetcher,
    const net::CookieList& cookies) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> joa =
      Java_CookiesFetcher_createCookiesArray(env, cookies.size());

  int index = 0;
  for (const auto& cookie : cookies) {
    // TODO (crbug.com/326605834) Once ancestor chain bit changes are
    // implemented update this method utilize the ancestor bit.
    base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        key_serialized_result =
            net::CookiePartitionKey::Serialize(cookie.PartitionKey());
    if (!key_serialized_result.has_value()) {
      continue;
    }
    ScopedJavaLocalRef<jobject> java_cookie = Java_CookiesFetcher_createCookie(
        env, base::android::ConvertUTF8ToJavaString(env, cookie.Name()),
        base::android::ConvertUTF8ToJavaString(env, cookie.Value()),
        base::android::ConvertUTF8ToJavaString(env, cookie.Domain()),
        base::android::ConvertUTF8ToJavaString(env, cookie.Path()),
        cookie.CreationDate().ToDeltaSinceWindowsEpoch().InMicroseconds(),
        cookie.ExpiryDate().ToDeltaSinceWindowsEpoch().InMicroseconds(),
        cookie.LastAccessDate().ToDeltaSinceWindowsEpoch().InMicroseconds(),
        cookie.LastUpdateDate().ToDeltaSinceWindowsEpoch().InMicroseconds(),
        cookie.SecureAttribute(), cookie.IsHttpOnly(),
        static_cast<int>(cookie.SameSite()), cookie.Priority(),
        base::android::ConvertUTF8ToJavaString(
            env, key_serialized_result->TopLevelSite()),
        static_cast<int>(cookie.SourceScheme()), cookie.SourcePort(),
        static_cast<int>(cookie.SourceType()));
    env->SetObjectArrayElement(joa.obj(), index++, java_cookie.obj());
  }

  Java_CookiesFetcher_onCookieFetchFinished(env, j_cookie_fetcher, joa);
}

}  // namespace

std::string JNI_CookiesFetcher_GetCookieFileDirectory(JNIEnv* env,
                                                      Profile* profile) {
  return profile->GetPath().Append(chrome::kOTRTempStateDirname).value();
}

// Fetches cookies for the off-the-record session (i.e. incognito mode). It is a
// no-op for the standard session. Typically associated with the #onPause of
// Android's activity lifecycle.
void JNI_CookiesFetcher_PersistCookies(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& j_cookies_fetcher) {
  cookie_fetcher_restore_util::GetCookieServiceClient(profile)->GetAllCookies(
      base::BindOnce(&OnCookiesFetchFinished,
                     ScopedJavaGlobalRef<jobject>(j_cookies_fetcher)));
}

void JNI_CookiesFetcher_RestoreCookies(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jstring>& name,
    const JavaParamRef<jstring>& value,
    const JavaParamRef<jstring>& domain,
    const JavaParamRef<jstring>& path,
    jlong creation,
    jlong expiration,
    jlong last_access,
    jlong last_update,
    jboolean secure,
    jboolean httponly,
    jint same_site,
    jint priority,
    const JavaParamRef<jstring>& partition_key,
    jint source_scheme,
    jint source_port,
    jint source_type) {
  cookie_fetcher_restore_util::CookiesFetcherRestoreCookiesImpl(
      env, profile, name, value, domain, path, creation, expiration,
      last_access, last_update, secure, httponly, same_site, priority,
      partition_key, source_scheme, source_port, source_type);
}
