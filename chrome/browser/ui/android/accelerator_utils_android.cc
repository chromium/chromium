// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/accelerator_utils.h"

#include <algorithm>
#include <string>

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/accelerator_table.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/android/events_android_utils.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/event.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/KeyboardShortcuts_jni.h"

bool IsChromeAccelerator(const ui::Accelerator& accelerator) {
  if (base::FeatureList::IsEnabled(
          chrome::android::kUniversalKeyboardHandling)) {
    return std::ranges::any_of(
        GetAcceleratorList(), [&](const AcceleratorMapping& entry) {
          return entry.keycode == accelerator.key_code() &&
                 entry.modifiers == accelerator.modifiers();
        });
  }

  ui::KeyEvent key_event = accelerator.ToKeyEvent();
  ui::PlatformEvent platform_event = ui::NativeEventFromEvent(key_event);

  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_KeyboardShortcuts_isChromeAccelerator(
      env, platform_event.AsKeyboardEventAndroid()->GetJavaObject());
}

static std::u16string JNI_KeyboardShortcuts_GetShortcutTextForCommandId(
    JNIEnv* env,
    int command_id) {
  ui::Accelerator accelerator;
  if (!GetAcceleratorForCommandId(command_id, &accelerator)) {
    return std::u16string();
  }
  return accelerator.GetShortcutText();
}

DEFINE_JNI(KeyboardShortcuts)
