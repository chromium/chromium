// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The native side of TtcShortcutUtils, which uses ui::Accelerator for TTC's
// keyboard shortcuts, like Chrome's own shortcuts.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ttc/core/shortcut.h"
#include "ui/android/accelerator_manager_android.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ttc/android/settings_jni_headers/TtcShortcutUtils_jni.h"

namespace ttc {

static std::string JNI_TtcShortcutUtils_FromKeyEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_key_event) {
  std::optional<ui::Accelerator> accelerator =
      ui::AcceleratorManagerAndroid::AcceleratorFromKeyEvent(j_key_event);
  if (!accelerator) {
    return std::string();
  }
  // Only a key press that the pref value names exactly, so that what is saved
  // is what was pressed.
  std::string shortcut = ui::Command::AcceleratorToString(*accelerator);
  if (ui::Command::StringToAccelerator(shortcut) != *accelerator) {
    return std::string();
  }
  return shortcut;
}

static int32_t JNI_TtcShortcutUtils_GetStatus(JNIEnv* env,
                                              const std::string& shortcut) {
  // A value that names no shortcut parses to an empty accelerator, which is
  // invalid.
  return static_cast<int32_t>(
      ValidateShortcut(ui::Command::StringToAccelerator(shortcut)));
}

static std::string JNI_TtcShortcutUtils_Canonicalize(
    JNIEnv* env,
    const std::string& shortcut) {
  // Several values can name the same shortcut, e.g. with the modifiers in
  // another order. This is the one FromKeyEvent() gives.
  ui::Accelerator accelerator = ParseShortcut(shortcut);
  if (accelerator.IsEmpty()) {
    return std::string();
  }
  return ui::Command::AcceleratorToString(accelerator);
}

static std::vector<std::u16string> JNI_TtcShortcutUtils_GetKeyLabels(
    JNIEnv* env,
    const std::string& shortcut) {
  return ui::Command::StringToAccelerator(shortcut)
      .GetShortcutVectorRepresentation();
}

}  // namespace ttc

DEFINE_JNI(TtcShortcutUtils)
