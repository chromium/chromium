// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_settings_navigation_android.h"

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicSettingsNavigation_jni.h"

namespace glic {

void ShowGlicSettings() {
  Java_GlicSettingsNavigation_showGlicSettings(
      base::android::AttachCurrentThread());
}

}  // namespace glic
