// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/authenticator_chromeos.h"

#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "chrome/browser/ash/auth/legacy_fingerprint_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/components/in_session_auth/mojom/in_session_auth.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void OnRequestToken(base::OnceCallback<void(bool)> callback,
                    chromeos::auth::mojom::RequestTokenReplyPtr reply) {
  // Similarly to `OnAuthComplete`, we ignore the token provided in reply, if
  // any.
  std::move(callback).Run(
      reply !=
      mojo::StructPtr<chromeos::auth::mojom::RequestTokenReply>(nullptr));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::InSessionAuthDialogController::Get()->ShowAuthDialog(
      ash::InSessionAuthDialogController::Reason::kAccessPasswordManager,
      base::UTF16ToUTF8(message),
      base::BindOnce(&OnAuthComplete, std::move(result_callback)));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (auto* lacros_service = chromeos::LacrosService::Get();
      lacros_service->IsAvailable<chromeos::auth::mojom::InSessionAuth>()) {
    if (lacros_service
            ->GetInterfaceVersion<chromeos::auth::mojom::InSessionAuth>() <
        static_cast<int>(chromeos::auth::mojom::InSessionAuth::
                             MethodMinVersions::kRequestTokenMinVersion)) {
      lacros_service->GetRemote<chromeos::auth::mojom::InSessionAuth>()
          ->RequestToken(
              chromeos::auth::mojom::Reason::kAccessPasswordManager,
              std::nullopt,
              base::BindOnce(&OnRequestToken, std::move(result_callback)));
    } else {
      lacros_service->GetRemote<chromeos::auth::mojom::InSessionAuth>()
          ->RequestToken(
              chromeos::auth::mojom::Reason::kAccessPasswordManager,
              base::UTF16ToUTF8(message),
              base::BindOnce(&OnRequestToken, std::move(result_callback)));
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

BiometricsStatusChromeOS AuthenticatorChromeOS::CheckIfBiometricsAvailable() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#else
  return BiometricsStatusChromeOS::kUnavailable;
#endif
}
