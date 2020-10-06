// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ANDROID_VR_UTILS_H_
#define CHROME_BROWSER_ANDROID_VR_ANDROID_VR_UTILS_H_

#include "base/android/jni_android.h"

namespace content {
class WebContents;
}

// Functions in this file are currently GVR/ArCore specific functions. If other
// platforms need the same function here, please move it to
// chrome/browser/vr/*util.cc|h
namespace vr {

content::WebContents* GetWebContents(int render_process_id,
                                     int render_frame_id);

base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents(
    int render_process_id,
    int render_frame_id);

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ANDROID_VR_UTILS_H_
