// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_jni_onload.h"

#include "chrome/app/android/chrome_main_delegate_android.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"

namespace android {

bool OnJNIOnLoadInit() {
  if (!content::android::OnJNIOnLoadInit())
    return false;

  content::SetContentMainDelegate(new ChromeMainDelegateAndroid());
  return true;
}

}  // namespace android
