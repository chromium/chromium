// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_service_factory_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/data_sharing/jni_headers/DataSharingServiceFactoryBridge_jni.h"
#include "chrome/browser/profiles/profile.h"

namespace data_sharing {

DataSharingServiceFactoryBridge::DataSharingServiceFactoryBridge() = default;
DataSharingServiceFactoryBridge::~DataSharingServiceFactoryBridge() = default;

// static
ScopedJavaLocalRef<jobject>
DataSharingServiceFactoryBridge::CreateJavaSDKDelegate(Profile* profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_profile = profile->GetJavaObject();
  return Java_DataSharingServiceFactoryBridge_createJavaSDKDelegate(env,
                                                                    j_profile);
}

}  // namespace data_sharing
