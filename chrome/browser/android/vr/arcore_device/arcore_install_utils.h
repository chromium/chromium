// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_UTILS_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_UTILS_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"

namespace vr {

class ArCoreInstallUtils {
 public:
  virtual ~ArCoreInstallUtils() = default;
  virtual bool ShouldRequestInstallArModule() = 0;
  virtual void RequestInstallArModule() = 0;
  virtual bool ShouldRequestInstallSupportedArCore() = 0;
  virtual void RequestInstallSupportedArCore(int render_process_id,
                                             int render_frame_id) = 0;
  virtual bool EnsureLoaded() = 0;
  virtual base::android::ScopedJavaLocalRef<jobject>
  GetApplicationContext() = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_UTILS_H_
