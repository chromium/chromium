// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/chime/android/features.h"
#include "chrome/browser/notifications/chime/android/jni_headers/ChimeSession_jni.h"

jboolean JNI_ChimeSession_IsEnabled(JNIEnv* env) {
  return base::FeatureList::IsEnabled(
      notifications::features::kUseChimeAndroidSdk);
}

namespace notifications {
namespace features {

const base::Feature kUseChimeAndroidSdk{"UseChimeAndroidSdk",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace notifications
