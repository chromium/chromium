// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_settings_util.h"

#include <string>

#include "base/notimplemented.h"
#include "chrome/browser/glic/android/glic_navigation_utils_android.h"

namespace glic {

void OpenGlicSettingsPage(Profile* profile) {
  ShowGlicSettings();
}

void OpenGlicOsToggleSetting(Profile* profile) {
  ShowGlicSettings();
}

void OpenGlicKeyboardShortcutSetting(Profile* profile) {
  ShowGlicSettings();
}

void OpenPasswordManagerSettingsPage(Profile* profile) {
  NOTIMPLEMENTED();
}

std::string_view GetPlatformHelpSuffix() {
  return "_android";
}

}  // namespace glic
