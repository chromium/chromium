// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/authenticator_chromeos.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/auth/legacy_fingerprint_engine.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/prefs/pref_service.h"

namespace {
void OnAuthComplete(base::OnceCallback<void(bool)> callback,
                    bool success,
                    const ash::AuthProofToken& token,
                    base::TimeDelta timeout) {
  // Here we simply ignore `token` and `timeout`, as password manager manages
  // its own auth timeout
  std::move(callback).Run(success);
}

bool HasFingerprintRecord(const PrefService& pref_service) {
  return pref_service.GetInteger(ash::prefs::kQuickUnlockFingerprintRecord) !=
         0;
}
}  // namespace

AuthenticatorChromeOS::AuthenticatorChromeOS() = default;

AuthenticatorChromeOS::~AuthenticatorChromeOS() = default;

void AuthenticatorChromeOS::AuthenticateUser(
    const std::u16string& message,
    base::OnceCallback<void(bool)> result_callback) {
  // Calls `InSessionAuthDialogController::ShowAuthDialog` to authenticate the
  // currently active user using configured auth factors.
  // On Lacros, makes a crosapi call to the
  // `chromeos::auth::mojom::InSessionAuth` interface implemented by ash. This
  // in turn calls `InSessionAuthDialogController::ShowAuthDialog` to
  // authenticate the currently active user using configured auth factors.
  ash::InSessionAuthDialogController::Get()->ShowAuthDialog(
      ash::InSessionAuthDialogController::Reason::kAccessPasswordManager,
      base::UTF16ToUTF8(message),
      base::BindOnce(&OnAuthComplete, std::move(result_callback)));
}

BiometricsStatusChromeOS AuthenticatorChromeOS::CheckIfBiometricsAvailable() {
  const PrefService& prefs =
      *ProfileManager::GetActiveUserProfile()->GetPrefs();

  // No need for an AuthPerformer since we don't intent to perform
  // authentication here.
  ash::LegacyFingerprintEngine fp_engine(nullptr);
  bool is_fingerprint_enabled = fp_engine.IsFingerprintEnabled(
      prefs, ash::LegacyFingerprintEngine::Purpose::kAny);

  if (!is_fingerprint_enabled) {
    return BiometricsStatusChromeOS::kUnavailable;
  }

  return HasFingerprintRecord(prefs)
             ? BiometricsStatusChromeOS::kAvailable
             : BiometricsStatusChromeOS::kNotConfiguredForUser;
}
