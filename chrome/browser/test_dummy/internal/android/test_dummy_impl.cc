// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/test_dummy/internal/jni_headers/TestDummyImpl_jni.h"
#include "chrome/grit/test_dummy_resources.h"
#include "ui/base/resource/resource_bundle.h"

static int JNI_TestDummyImpl_Execute(JNIEnv* env) {
  LOG(INFO) << "Running test dummy native library";
  return 123;
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_TestDummyImpl_LoadResource(JNIEnv* env) {
  auto resource = ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_TEST_DUMMY_TEST_RESOURCE);
  LOG(INFO) << "Loading dummy native resource: " << resource;
  return base::android::ConvertUTF8ToJavaString(env, resource);
}
