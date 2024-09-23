// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"

#include "base/android/build_info.h"
#include "base/android/scoped_java_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/android/channel_getter.h"
#include "ui/android/window_android.h"
#include "ui/gfx/native_widget_types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PasswordMigrationWarningBridge_jni.h"

using base::android::AttachCurrentThread;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState;

namespace local_password_migration {

constexpr base::TimeDelta kMinIntervalBetweenWarnings = base::Days(1);

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
      AttachCurrentThread(), window->GetJavaObject(), profile->GetJavaObject(),
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
      profile->GetJavaObject(), static_cast<int>(trigger_source));

  RecordPasswordMigrationWarningTriggerSource(trigger_source);
}

bool ShouldShowWarning(Profile* profile) {
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    return false;
  }
  // If we're showing the access loss warnings, there is no need to show the
  // migration warning anymore.
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning)) {
    return false;
  }
  if (password_manager::UsesSplitStoresAndUPMForLocal(profile->GetPrefs())) {
    return false;
  }
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

  if (password_manager::sync_util::HasChosenToSyncPasswords(
          SyncServiceFactory::GetForProfile(profile))) {
    // No signed-in / syncing users with password sync enabled should see the
    // warning. This is an oversimplification to avoid confusion, in reality
    // some users in this group *do* save to LoginDatabase (e.g. if GmsCore is
    // outdated).
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

void MaybeShowPostMigrationSheet(const gfx::NativeWindow window,
                                 Profile* profile) {
  if (!window) {
    return;
  }
  if (!profile) {
    return;
  }
  if (!ShouldShowPostMigrationSheet(profile)) {
    return;
  }

  Java_PasswordMigrationWarningBridge_maybeShowPostMigrationSheet(
      AttachCurrentThread(), window->GetJavaObject(), profile->GetJavaObject());
}

bool ShouldShowPostMigrationSheet(Profile* profile) {
  // Don't show in incognito.
  if (profile->IsOffTheRecord()) {
    return false;
  }

  // The sheet should only show on non-stable channels.
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE) {
    return false;
  }

  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    return false;
  }

  // The post password migration sheet should be shown for an active UPM user
  // that uses split stores only once, so
  // `kLocalPasswordMigrationWarningShownAtStartup` will be true only when the
  // migration algorithm sets it to true and it will be flipped to false when
  // the sheet is shown.
  return profile->GetPrefs()->GetBoolean(
      password_manager::prefs::kShouldShowPostPasswordMigrationSheetAtStartup);
}

}  // namespace local_password_migration
