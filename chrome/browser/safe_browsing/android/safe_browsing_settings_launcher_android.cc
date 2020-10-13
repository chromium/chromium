// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/safe_browsing_settings_launcher_android.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/SafeBrowsingSettingsLauncher_jni.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

void ShowSafeBrowsingSettings(content::WebContents* web_contents) {
  Java_SafeBrowsingSettingsLauncher_showSafeBrowsingSettings(
      base::android::AttachCurrentThread(), web_contents->GetJavaWebContents());
}

}  // namespace safe_browsing
