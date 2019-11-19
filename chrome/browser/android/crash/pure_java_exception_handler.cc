// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/crash/pure_java_exception_handler.h"

#include "chrome/android/chrome_jni_headers/PureJavaExceptionHandler_jni.h"

void UninstallPureJavaExceptionHandler() {
  Java_PureJavaExceptionHandler_uninstallHandler(
      base::android::AttachCurrentThread());
}
