// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_ANDROID_JNI_TRANSLATION_H_
#define CHROME_BROWSER_FEED_ANDROID_JNI_TRANSLATION_H_

#include <jni.h>
#include "base/android/jni_android.h"
#include "components/feed/core/v2/public/logging_parameters.h"

// JNI <-> C++ translation functions needed in multiple cc files.
namespace feed {
namespace android {

LoggingParameters ToNativeLoggingParameters(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& logging_parameters);
}

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_ANDROID_JNI_TRANSLATION_H_
