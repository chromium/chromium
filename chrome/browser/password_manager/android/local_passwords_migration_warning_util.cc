// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "chrome/browser/password_manager/android/pwd_migration/jni_headers/PasswordMigrationWarningBridge_jni.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;

namespace password_manager {
void ShowWarning(const gfx::NativeWindow window) {
  Java_PasswordMigrationWarningBridge_showWarning(AttachCurrentThread(),
                                                  window->GetJavaObject());
}
}  // namespace password_manager
