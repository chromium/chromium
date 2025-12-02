// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_PERMISSION_CONTROLLER_BRIDGE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_PERMISSION_CONTROLLER_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/web_contents.h"

namespace picture_in_picture {

// Native part of the Java AutoPictureInPicturePermissionController class. This
// header file defines the JNI functions that are called on the native side.
class AutoPictureInPicturePermissionControllerBridge {
 public:
  // Sets a flag to indicate that auto-pip has been triggered.
  static void SetAutoPipTriggered(content::WebContents& web_contents);

  // Clears the flag that indicates that auto-pip has been triggered.
  static void ClearAutoPipTriggered(content::WebContents& web_contents);

  // Clears any "Allow Once" state associated with the given WebContents.
  static void ClearAllowOnceState(content::WebContents& web_contents);
};

}  // namespace picture_in_picture

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_PERMISSION_CONTROLLER_BRIDGE_H_
