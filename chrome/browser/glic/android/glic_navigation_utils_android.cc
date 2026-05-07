// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_navigation_utils_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicNavigationUtils_jni.h"

namespace glic {

void ShowGlicSettings() {
  Java_GlicNavigationUtils_showGlicSettings(
      base::android::AttachCurrentThread());
}

void ShowSignIn(Profile* profile, content::WebContents* web_contents) {
  Java_GlicNavigationUtils_showSignIn(
      base::android::AttachCurrentThread(), profile->GetJavaObject(),
      web_contents ? web_contents->GetJavaWebContents() : nullptr);
}

}  // namespace glic
