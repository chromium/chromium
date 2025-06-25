// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/omnibox_prefs.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/composeplate/android/jni_headers/ComposeplateUtils_jni.h"

// static
jboolean JNI_ComposeplateUtils_IsEnabledByPolicy(JNIEnv* env,
                                                 Profile* profile) {
  DCHECK(profile);

  return omnibox::IsMiaAllowedByPolicy(profile->GetPrefs());
}
