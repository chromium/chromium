// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_service_binding_batch.h"

#include "base/android/jni_android.h"
#include "base/process_launcher_jni/ScopedServiceBindingBatch_jni.h"

namespace base::android {

ScopedServiceBindingBatch::ScopedServiceBindingBatch() {
  JNIEnv* env = base::android::AttachCurrentThread();
  // The Java method can return null if the feature is not activated.
  base::android::ScopedJavaLocalRef<jobject> java_scoped_batch_update =
      Java_ScopedServiceBindingBatch_scoped(env);
  if (java_scoped_batch_update) {
    java_object_.Reset(java_scoped_batch_update);
  }
}

ScopedServiceBindingBatch::~ScopedServiceBindingBatch() {
  // If java_object_ is null, it means the batch update was not started,
  // so there is no need to close it.
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ScopedServiceBindingBatch_close(env, java_object_);
  }
}

}  // namespace base::android

DEFINE_JNI(ScopedServiceBindingBatch)
