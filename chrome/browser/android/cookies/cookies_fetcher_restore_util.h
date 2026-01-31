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
void CookiesFetcherRestoreCookiesImpl(JNIEnv* env,
                                      Profile* profile,
                                      const std::string& name,
                                      const std::string& value,
                                      const std::string& domain,
                                      const std::string& path,
                                      int64_t creation,
                                      int64_t expiration,
                                      int64_t last_access,
                                      int64_t last_update,
                                      bool secure,
                                      bool httponly,
                                      int32_t same_site,
                                      int32_t priority,
                                      const std::string& partition_key,
                                      int32_t source_scheme,
                                      int32_t source_port,
                                      int32_t source_type);

}  // namespace cookie_fetcher_restore_util

#endif  // CHROME_BROWSER_ANDROID_COOKIES_COOKIES_FETCHER_RESTORE_UTIL_H_
