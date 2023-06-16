// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "chrome/android/chrome_jni_headers/PasswordMigrationWarningBridge_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;

namespace password_manager {
void ShowWarning(const gfx::NativeWindow window, Profile* profile) {
  Java_PasswordMigrationWarningBridge_showWarning(
      AttachCurrentThread(), window->GetJavaObject(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject());
}

bool ShouldShowWarning() {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsMigrationWarning)) {
    return false;
  }
  // TODO(crbug.com/1451827): Implement the actual logic whether to show the
  // warning here.
  return true;
}
}  // namespace password_manager
