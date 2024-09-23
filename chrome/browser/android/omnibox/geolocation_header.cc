// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/geolocation_header.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/GeolocationHeader_jni.h"

bool HasGeolocationPermission() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_GeolocationHeader_hasGeolocationPermission(env);
}

std::optional<std::string> GetGeolocationHeaderIfAllowed(const GURL& url,
                                                         Profile* profile) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobject> j_profile_android =
      profile->GetJavaObject();

  base::android::ScopedJavaLocalRef<jstring> geo_header =
      Java_GeolocationHeader_getGeoHeader(
          env, base::android::ConvertUTF8ToJavaString(env, url.spec()),
          j_profile_android);

  if (!geo_header)
    return std::nullopt;

  return base::android::ConvertJavaStringToUTF8(env, geo_header);
}
