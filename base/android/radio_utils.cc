// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/radio_utils.h"

#include "base/base_jni_headers/RadioUtils_jni.h"

namespace base {
namespace android {

bool RadioUtils::IsSupported() {
  JNIEnv* env = AttachCurrentThread();
  return Java_RadioUtils_isSupported(env);
}

bool RadioUtils::IsWifiConnected() {
  JNIEnv* env = AttachCurrentThread();
  return Java_RadioUtils_isWifiConnected(env);
}

Optional<RadioSignalLevel> RadioUtils::GetCellSignalLevel() {
  JNIEnv* env = AttachCurrentThread();
  int signal_level = Java_RadioUtils_getCellSignalLevel(env);
  if (signal_level < 0) {
    return nullopt;
  } else {
    return static_cast<RadioSignalLevel>(signal_level);
  }
}

RadioDataActivity RadioUtils::GetCellDataActivity() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<RadioDataActivity>(
      Java_RadioUtils_getCellDataActivity(env));
}

}  // namespace android
}  // namespace base
