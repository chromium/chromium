// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/grit/test_dummy_resources.h"
#include "ui/base/resource/resource_bundle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/test_dummy/internal/jni_headers/TestDummyImpl_jni.h"

static int JNI_TestDummyImpl_Execute(JNIEnv* env, jboolean arg) {
  CHECK(arg);
  LOG(INFO) << "Running test dummy native library";
  return 123;
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_TestDummyImpl_LoadResource(JNIEnv* env) {
  auto resource =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_TEST_DUMMY_TEST_RESOURCE);
  LOG(INFO) << "Loading dummy native resource: " << resource;
  return base::android::ConvertUTF8ToJavaString(env, resource);
}
