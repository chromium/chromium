// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_PREVIEW_DATA_SERVICE_ANDROID_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_PREVIEW_DATA_SERVICE_ANDROID_H_

#include "base/android/scoped_java_ref.h"

namespace signin {
class AccountPreviewDataService;
}

namespace jni_zero {

template <>
base::android::ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    signin::AccountPreviewDataService* service);

}  // namespace jni_zero

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_PREVIEW_DATA_SERVICE_ANDROID_H_
