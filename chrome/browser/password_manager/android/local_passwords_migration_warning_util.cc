// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/PasswordMigrationWarningBridge_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;

namespace local_password_migration {

constexpr base::TimeDelta kMinIntervalBetweenWarnings = base::Days(30);

void SaveWarningShownTimestamp(PrefService* pref_service) {
  pref_service->SetTime(
      password_manager::prefs::kLocalPasswordsMigrationWarningShownTimestamp,
      base::Time::Now());
}

void ShowWarning(const gfx::NativeWindow window, Profile* profile) {
  if (!ShouldShowWarning(profile)) {
    return;
  }
  Java_PasswordMigrationWarningBridge_showWarning(
      AttachCurrentThread(), window->GetJavaObject(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject());
  SaveWarningShownTimestamp(profile->GetPrefs());
}

bool ShouldShowWarning(Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsMigrationWarning)) {
    return false;
  }

  if (password_manager::features::kIgnoreMigrationWarningTimeout.Get()) {
    return true;
  }

  PrefService* pref_service = profile->GetPrefs();
  base::Time last_shown_timestamp = pref_service->GetTime(
      password_manager::prefs::kLocalPasswordsMigrationWarningShownTimestamp);
  base::TimeDelta time_since_last_shown =
      base::Time::Now() - last_shown_timestamp;
  if (time_since_last_shown < kMinIntervalBetweenWarnings) {
    return false;
  }

  // TODO(crbug.com/1451827): Check whether the user is already syncing
  // passwords.
  return true;
}

}  // namespace local_password_migration
