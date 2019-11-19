// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_context_util.h"

#include "base/android/jni_android.h"
#include "chrome/browser/util/jni_headers/ChromeContextUtil_jni.h"

namespace chrome {
namespace android {

ChromeContextUtil::ChromeContextUtil() {}

// static
int ChromeContextUtil::GetSmallestDIPWidth() {
  return Java_ChromeContextUtil_getSmallestDIPWidth(
      base::android::AttachCurrentThread());
}

}  // namespace android
}  // namespace chrome
