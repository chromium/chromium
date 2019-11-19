// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// Key under "kiosk" dictionary to store the last launch error.
constexpr char kKeyLaunchError[] = "launch_error";

// Key under "kiosk" dictionary to store the last cryptohome error.
constexpr char kKeyCryptohomeFailure[] = "cryptohome_failure";

// Error from the last kiosk launch.
KioskAppLaunchError::Error s_last_error = KioskAppLaunchError::ERROR_COUNT;

}  // namespace

// static
std::string KioskAppLaunchError::GetErrorMessage(Error error) {
  switch (error) {
    case NONE:
      return std::string();

    case HAS_PENDING_LAUNCH:
    case NOT_KIOSK_ENABLED:
    case UNABLE_TO_RETRIEVE_HASH:
    case POLICY_LOAD_FAILED:
    case ARC_AUTH_FAILED:
      return l10n_util::GetStringUTF8(IDS_KIOSK_APP_FAILED_TO_LAUNCH);

    case CRYPTOHOMED_NOT_RUNNING:
    case ALREADY_MOUNTED:
    case UNABLE_TO_MOUNT:
    case UNABLE_TO_REMOVE:
      return l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_MOUNT);

    case UNABLE_TO_INSTALL:
      return l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_INSTALL);

    case USER_CANCEL:
      return l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_USER_CANCEL);

    case UNABLE_TO_DOWNLOAD:
      return l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_DOWNLOAD);

    case UNABLE_TO_LAUNCH:
      return l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_LAUNCH);

    case ERROR_COUNT:
      // Break onto the NOTREACHED() code path below.
      break;
  }

  NOTREACHED() << "Unknown kiosk app launch error, error=" << error;
  return l10n_util::GetStringUTF8(IDS_KIOSK_APP_FAILED_TO_LAUNCH);
}

// static
void KioskAppLaunchError::Save(KioskAppLaunchError::Error error) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state,
                                   KioskAppManager::kKioskDictionaryName);
  dict_update->SetInteger(kKeyLaunchError, error);
  s_last_error = error;
}

// static
void KioskAppLaunchError::SaveCryptohomeFailure(
    const AuthFailure& auth_failure) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state,
                                   KioskAppManager::kKioskDictionaryName);
  dict_update->SetInteger(kKeyCryptohomeFailure, auth_failure.reason());
}

// static
KioskAppLaunchError::Error KioskAppLaunchError::Get() {
  if (s_last_error != KioskAppLaunchError::ERROR_COUNT)
    return s_last_error;
  s_last_error = KioskAppLaunchError::NONE;
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dict =
      local_state->GetDictionary(KioskAppManager::kKioskDictionaryName);

  int error;
  if (dict->GetInteger(kKeyLaunchError, &error)) {
    s_last_error = static_cast<KioskAppLaunchError::Error>(error);
    return s_last_error;
  }

  return KioskAppLaunchError::NONE;
}

// static
void KioskAppLaunchError::RecordMetricAndClear() {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state,
                                   KioskAppManager::kKioskDictionaryName);

  int error;
  if (dict_update->GetInteger(kKeyLaunchError, &error))
    UMA_HISTOGRAM_ENUMERATION("Kiosk.Launch.Error", static_cast<Error>(error),
                              ERROR_COUNT);
  dict_update->Remove(kKeyLaunchError, NULL);

  int cryptohome_failure;
  if (dict_update->GetInteger(kKeyCryptohomeFailure, &cryptohome_failure)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Kiosk.Launch.CryptohomeFailure",
        static_cast<AuthFailure::FailureReason>(cryptohome_failure),
        AuthFailure::NUM_FAILURE_REASONS);
  }
  dict_update->Remove(kKeyCryptohomeFailure, NULL);
}

}  // namespace chromeos
