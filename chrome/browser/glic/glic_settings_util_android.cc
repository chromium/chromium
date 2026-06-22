// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_settings_util.h"

#include <string>

#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "chrome/browser/glic/android/glic_navigation_utils_android.h"
#include "components/password_manager/core/browser/features/password_features.h"

namespace glic {

void OpenGlicSettingsPage(Profile* profile) {
  ShowGlicSettings(GlicSettingsPage::kMain);
}

void OpenGlicOsToggleSetting(Profile* profile) {
  ShowGlicSettings(GlicSettingsPage::kMain);
}

void OpenGlicKeyboardShortcutSetting(Profile* profile) {
  ShowGlicSettings(GlicSettingsPage::kMain);
}

void OpenPasswordManagerSettingsPage(Profile* profile) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginPermissionsUi)) {
    ShowGlicSettings(GlicSettingsPage::kActorLoginPermissions);
  } else {
    NOTIMPLEMENTED();
  }
}

std::string_view GetPlatformHelpSuffix() {
  return "_android";
}

}  // namespace glic
