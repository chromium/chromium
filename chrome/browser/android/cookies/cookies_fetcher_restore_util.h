// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COOKIES_COOKIES_FETCHER_RESTORE_UTIL_H_
#define CHROME_BROWSER_ANDROID_COOKIES_COOKIES_FETCHER_RESTORE_UTIL_H_

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"

class Profile;

namespace cookie_fetcher_restore_util {

// Returns the cookie service at the client end of the mojo pipe.
network::mojom::CookieManager* GetCookieServiceClient(Profile* profile);

// Creates and sets a canonical cookie for the off-the-record session (i.e.
// incognito mode). It is a no-op for the standard session. Typically associated
// with the #onResume of Android's activity lifecycle.
void CookiesFetcherRestoreCookiesImpl(
    JNIEnv* env,
    Profile* profile,
    const jni_zero::JavaParamRef<jstring>& name,
    const jni_zero::JavaParamRef<jstring>& value,
    const jni_zero::JavaParamRef<jstring>& domain,
    const jni_zero::JavaParamRef<jstring>& path,
    jlong creation,
    jlong expiration,
    jlong last_access,
    jlong last_update,
    jboolean secure,
    jboolean httponly,
    jint same_site,
    jint priority,
    const jni_zero::JavaParamRef<jstring>& partition_key,
    jint source_scheme,
    jint source_port,
    jint source_type);

}  // namespace cookie_fetcher_restore_util

#endif  // CHROME_BROWSER_ANDROID_COOKIES_COOKIES_FETCHER_RESTORE_UTIL_H_
