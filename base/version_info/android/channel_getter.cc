// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version_info/android/channel_getter.h"

#include <optional>

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/version_info/android/version_constants_bridge_jni/VersionConstantsBridge_jni.h"

namespace version_info {
namespace android {
namespace {
std::optional<Channel> g_cached_channel;
}  // namespace

void SetChannel(Channel channel) {
  g_cached_channel = channel;
}

Channel GetChannel() {
  if (!g_cached_channel.has_value()) {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    g_cached_channel =
        static_cast<Channel>(Java_VersionConstantsBridge_getChannel(env));
  }
  return g_cached_channel.value();
}

static void JNI_VersionConstantsBridge_NativeSetChannel(JNIEnv* env,
                                                        jint channel) {
  SetChannel(static_cast<Channel>(channel));
}

}  // namespace android
}  // namespace version_info

DEFINE_JNI(VersionConstantsBridge)
