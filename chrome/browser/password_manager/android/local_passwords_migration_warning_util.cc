// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"

#include "base/android/scoped_java_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/PasswordMigrationWarningBridge_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/android/channel_getter.h"
#include "ui/android/window_android.h"
#include "ui/gfx/native_widget_types.h"

using base::android::AttachCurrentThread;

namespace local_password_migration {

constexpr base::TimeDelta kMinIntervalBetweenWarnings = base::Days(30);

void SaveWarningShownTimestamp(PrefService* pref_service) {
  pref_service->SetTime(
      password_manager::prefs::kLocalPasswordsMigrationWarningShownTimestamp,
      base::Time::Now());
}

void RecordPasswordMigrationWarningTriggerSource(
    password_manager::metrics_util::PasswordMigrationWarningTriggers
        trigger_source) {
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordMigrationWarning.Trigger", trigger_source);
}

void ShowWarning(
    const gfx::NativeWindow window,
    Profile* profile,
    password_manager::metrics_util::PasswordMigrationWarningTriggers
        trigger_source) {
  if (!window) {
    return;
  }
  if (!ShouldShowWarning(profile)) {
    return;
  }
  SaveWarningShownTimestamp(profile->GetPrefs());

  Java_PasswordMigrationWarningBridge_showWarning(
      AttachCurrentThread(), window->GetJavaObject(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject(),
      static_cast<int>(trigger_source));

  RecordPasswordMigrationWarningTriggerSource(trigger_source);
}

void ShowWarningWithActivity(
    const base::android::JavaParamRef<jobject>& activity,
    const base::android::JavaParamRef<jobject>& bottom_sheet_controller,
    Profile* profile,
    password_manager::metrics_util::PasswordMigrationWarningTriggers
        trigger_source) {
  if (!ShouldShowWarning(profile)) {
    return;
  }
  SaveWarningShownTimestamp(profile->GetPrefs());

  Java_PasswordMigrationWarningBridge_showWarningWithActivity(
      AttachCurrentThread(), activity, bottom_sheet_controller,
      ProfileAndroid::FromProfile(profile)->GetJavaObject(),
      static_cast<int>(trigger_source));

  RecordPasswordMigrationWarningTriggerSource(trigger_source);
}

bool ShouldShowWarning(Profile* profile) {
  // The warning should not show up on stable builds.
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE) {
    return false;
  }

  if (profile->IsOffTheRecord()) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsMigrationWarning)) {
    return false;
  }

  PrefService* pref_service = profile->GetPrefs();
  bool is_warning_acknowledged = pref_service->GetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning);
  if (is_warning_acknowledged) {
    return false;
  }

  // TODO(crbug.com/1466445): Migrate away from `ConsentLevel::kSync` on
  // Android.
  if (password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords(
          SyncServiceFactory::GetForProfile(profile))) {
    return false;
  }

  if (password_manager::features::kIgnoreMigrationWarningTimeout.Get()) {
    return true;
  }

  base::Time last_shown_timestamp = pref_service->GetTime(
      password_manager::prefs::kLocalPasswordsMigrationWarningShownTimestamp);
  base::TimeDelta time_since_last_shown =
      base::Time::Now() - last_shown_timestamp;
  if (time_since_last_shown < kMinIntervalBetweenWarnings) {
    return false;
  }

  return true;
}

}  // namespace local_password_migration
