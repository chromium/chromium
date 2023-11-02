// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/geolocation_header.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ui/android/omnibox/jni_headers/GeolocationHeader_jni.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

bool HasGeolocationPermission() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_GeolocationHeader_hasGeolocationPermission(env);
}

absl::optional<std::string> GetGeolocationHeaderIfAllowed(const GURL& url,
                                                          Profile* profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(profile);
  DCHECK(profile_android);

  base::android::ScopedJavaLocalRef<jobject> j_profile_android =
      profile_android->GetJavaObject();
  DCHECK(!j_profile_android.is_null());

  base::android::ScopedJavaLocalRef<jstring> geo_header =
      Java_GeolocationHeader_getGeoHeader(
          env, base::android::ConvertUTF8ToJavaString(env, url.spec()),
          j_profile_android);

  if (!geo_header)
    return absl::nullopt;

  return base::android::ConvertJavaStringToUTF8(env, geo_header);
}
