// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version_info/android/channel_getter.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/version_info/android/version_constants_bridge_jni/VersionConstantsBridge_jni.h"

namespace version_info {
namespace android {

Channel GetChannel() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return static_cast<Channel>(Java_VersionConstantsBridge_getChannel(env));
}

}  // namespace android
}  // namespace version_info
