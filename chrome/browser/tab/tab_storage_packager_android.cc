// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_packager_android.h"

#include "base/android/jni_android.h"
#include "base/token.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_packager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/TabStoragePackager_jni.h"

namespace tabs {

TabStoragePackagerAndroid::TabStoragePackagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      Java_TabStoragePackager_create(env, reinterpret_cast<intptr_t>(this)));
}

TabStoragePackagerAndroid::~TabStoragePackagerAndroid() = default;

}  // namespace tabs
