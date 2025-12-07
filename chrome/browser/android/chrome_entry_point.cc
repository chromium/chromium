// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/library_loader/library_loader_hooks.h"
#include "chrome/app/android/chrome_jni_onload.h"

bool NativeInitializationHook(
    base::android::LibraryProcessType library_process_type) {
  return android::OnJNIOnLoadInit();
}
