// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "chrome/android/chrome_jni_headers/PasswordAccessLossWarningBridge_jni.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "ui/android/window_android.h"

constexpr base::TimeDelta kMinIntervalBetweenWarnings = base::Days(1);
constexpr base::TimeDelta kMinIntervalBetweenWarningsAtStartup = base::Days(7);

PasswordAccessLossWarningBridgeImpl::PasswordAccessLossWarningBridgeImpl() =
    default;

PasswordAccessLossWarningBridgeImpl::~PasswordAccessLossWarningBridgeImpl() =
    default;

bool PasswordAccessLossWarningBridgeImpl::ShouldShowAccessLossNoticeSheet(
    PrefService* pref_service,
    bool called_at_startup) {
  // TODO: crbug.com/357063741 - Check all the criteria for showing the sheet.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning)) {
    return false;
  }

  if (password_manager_android_util::GetPasswordAccessLossWarningType(
          pref_service) ==
      password_manager_android_util::PasswordAccessLossWarningType::kNone) {
    return false;
  }

  if (password_manager::features::kIgnoreAccessLossWarningTimeout.Get()) {
    return true;
  }

  base::Time last_shown_timestamp = pref_service->GetTime(
      password_manager::prefs::kPasswordAccessLossWarningShownTimestamp);
  base::TimeDelta time_since_last_shown =
      base::Time::Now() - last_shown_timestamp;
  if (time_since_last_shown < kMinIntervalBetweenWarnings) {
    return false;
  }

  base::Time last_shown_timestamp_at_startup = pref_service->GetTime(
      password_manager::prefs::
          kPasswordAccessLossWarningShownAtStartupTimestamp);
  base::TimeDelta time_since_last_shown_at_startup =
      base::Time::Now() - last_shown_timestamp_at_startup;
  if (called_at_startup &&
      time_since_last_shown_at_startup < kMinIntervalBetweenWarningsAtStartup) {
    return false;
  }

  return true;
}

void PasswordAccessLossWarningBridgeImpl::MaybeShowAccessLossNoticeSheet(
    PrefService* pref_service,
    const gfx::NativeWindow window,
    Profile* profile,
    bool called_at_startup,
    password_manager_android_util::PasswordAccessLossWarningTriggers
        trigger_source) {
  if (profile == nullptr) {
    return;
  }
  if (!window) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> java_bridge =
      Java_PasswordAccessLossWarningBridge_create(env, window->GetJavaObject(),
                                                  profile->GetJavaObject());
  if (!java_bridge) {
    return;
  }

  password_manager_android_util::PasswordAccessLossWarningType warning_type =
      password_manager_android_util::GetPasswordAccessLossWarningType(
          pref_service);
  Java_PasswordAccessLossWarningBridge_show(env, java_bridge,
                                            static_cast<int>(warning_type));
  password_manager_android_util::RecordPasswordAccessLossWarningTriggerSource(
      trigger_source, warning_type);

  pref_service->SetTime(
      password_manager::prefs::kPasswordAccessLossWarningShownTimestamp,
      base::Time::Now());
  if (called_at_startup) {
    pref_service->SetTime(password_manager::prefs::
                              kPasswordAccessLossWarningShownAtStartupTimestamp,
                          base::Time::Now());
  }
}
