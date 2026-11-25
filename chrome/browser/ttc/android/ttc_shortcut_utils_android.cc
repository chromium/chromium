// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The native side of TtcShortcutUtils, which uses ui::Accelerator for TTC's
// keyboard shortcuts, like Chrome's own shortcuts.

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ttc/android/settings_jni_headers/TtcShortcutUtils_jni.h"

namespace ttc {

static std::vector<std::u16string> JNI_TtcShortcutUtils_GetKeyLabels(
    JNIEnv* env,
    const std::string& shortcut) {
  return ui::Command::StringToAccelerator(shortcut)
      .GetShortcutVectorRepresentation();
}

}  // namespace ttc

DEFINE_JNI(TtcShortcutUtils)
