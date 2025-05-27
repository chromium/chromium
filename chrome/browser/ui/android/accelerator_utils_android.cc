// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/accelerator_utils.h"

#include "ui/base/accelerators/accelerator.h"
#include "ui/events/android/events_android_utils.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/event.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/KeyboardShortcuts_jni.h"

namespace chrome {

bool IsChromeAccelerator(const ui::Accelerator& accelerator) {
  ui::KeyEvent key_event = accelerator.ToKeyEvent();
  ui::PlatformEvent platform_event = ui::NativeEventFromEvent(key_event);

  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_KeyboardShortcuts_isChromeAccelerator(
      env, platform_event.AsKeyboardEventAndroid()->GetJavaObject());
}

}  // namespace chrome
