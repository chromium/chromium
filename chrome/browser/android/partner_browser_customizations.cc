// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/partner_browser_customizations.h"

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/partnercustomizations/jni_headers/PartnerBrowserCustomizations_jni.h"

namespace chrome {
namespace android {

bool PartnerBrowserCustomizations::IsIncognitoDisabled() {
  return Java_PartnerBrowserCustomizations_isIncognitoDisabled(
      base::android::AttachCurrentThread());
}

}  // namespace android
}  // namespace chrome
