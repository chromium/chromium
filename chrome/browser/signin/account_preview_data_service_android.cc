// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_preview_data_service_android.h"

#include <optional>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/sync/base/data_type.h"
#include "google_apis/gaia/gaia_id.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after type headers used by JNI generated code.
#include "chrome/browser/signin/services/android/jni_headers/AccountPreviewDataService_jni.h"
#include "chrome/browser/signin/services/android/jni_headers/AccountPreviewPreference_jni.h"

namespace jni_zero {

template <>
ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    signin::AccountPreviewDataService* service) {
  if (!service) {
    return nullptr;
  }
  return signin::Java_AccountPreviewDataService_Constructor(
      env, reinterpret_cast<intptr_t>(service));
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    const signin::AccountPreviewDataService::AccountPreviewPreference&
        preference) {
  return signin::Java_AccountPreviewPreference_Constructor(
      env, preference.gaia_id, preference.preferred_data_types);
}

}  // namespace jni_zero

DEFINE_JNI(AccountPreviewDataService)
