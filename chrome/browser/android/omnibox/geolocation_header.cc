// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/geolocation_header.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/jni_zero/default_conversions.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/GeolocationHeader_jni.h"

std::optional<std::string> GetGeolocationHeaderIfAllowed(const GURL& url,
                                                         Profile* profile) {
  JNIEnv* env = base::android::AttachCurrentThread();

  return Java_GeolocationHeader_getGeoHeader(
      env, url.spec(), profile ? profile->GetJavaObject() : nullptr);
}

DEFINE_JNI(GeolocationHeader)
