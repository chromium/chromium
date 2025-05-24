// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"

#include "base/android/build_info.h"
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

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PasswordMigrationWarningBridge_jni.h"

using base::android::AttachCurrentThread;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState;

namespace local_password_migration {
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
  // `kShouldShowPostPasswordMigrationSheetAtStartup` will be true only when the
  // migration algorithm sets it to true and it will be flipped to false when
  // the sheet is shown.
  return profile->GetPrefs()->GetBoolean(
      password_manager::prefs::kShouldShowPostPasswordMigrationSheetAtStartup);
}

}  // namespace local_password_migration
