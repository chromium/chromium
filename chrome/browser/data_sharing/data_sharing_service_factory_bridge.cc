// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_service_factory_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/data_sharing/jni_headers/DataSharingServiceFactoryBridge_jni.h"

namespace data_sharing {

DataSharingServiceFactoryBridge::DataSharingServiceFactoryBridge() = default;
DataSharingServiceFactoryBridge::~DataSharingServiceFactoryBridge() = default;

// static
ScopedJavaLocalRef<jobject>
DataSharingServiceFactoryBridge::CreateJavaSDKDelegate() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DataSharingServiceFactoryBridge_createJavaSDKDelegate(env);
}

}  // namespace data_sharing
