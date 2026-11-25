// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/accelerator_utils.h"

#include <android/keycodes.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/accelerator_table.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/android/event_flags_android.h"
#include "ui/events/android/events_android_utils.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/events/keycodes/keyboard_codes.h"

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
  // Text-editing shortcuts are handled by Blink / focused text views rather
  // than AcceleratorManager, so they are omitted from kAcceleratorMap. Unlike
  // cut/copy/paste (covered by GetStandardAcceleratorForCommandId()), these two
  // only exist as renderer context-menu commands.
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      return ui::Accelerator(ui::VKEY_V,
                             ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)
          .GetShortcutText();
    case IDC_CONTENT_CONTEXT_SELECTALL:
      return ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN).GetShortcutText();
  }
  ui::Accelerator accelerator;
  if (!GetAcceleratorForCommandId(command_id, &accelerator)) {
    return std::u16string();
  }
  return accelerator.GetShortcutText();
}

// Returns flattened [command_id, android_key_code, android_meta_state] triples
// for the primary accelerator of each command in GetAcceleratorList().
static std::vector<int32_t> JNI_KeyboardShortcuts_GetAccelerators(JNIEnv* env) {
  std::vector<AcceleratorMapping> mappings = GetAcceleratorList();
  std::vector<int32_t> result;
  result.reserve(mappings.size() * 3);
  std::set<int> seen_commands;
  for (const AcceleratorMapping& entry : mappings) {
    if (!seen_commands.insert(entry.command_id).second) {
      continue;
    }
    int key_code = ui::AndroidKeyCodeFromKeyboardCode(entry.keycode);
    if (key_code == AKEYCODE_UNKNOWN) {
      continue;
    }
    result.push_back(entry.command_id);
    result.push_back(key_code);
    result.push_back(ui::AndroidMetaStateFromEventFlags(entry.modifiers));
  }
  return result;
}

DEFINE_JNI(KeyboardShortcuts)
