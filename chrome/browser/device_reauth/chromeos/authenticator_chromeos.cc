// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/authenticator_chromeos.h"

#include <algorithm>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/auth/legacy_fingerprint_engine.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

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

// Helper to check if the reply contains a PIN factor.
bool ContainsPinAuthFactor(
    std::optional<user_data_auth::ListAuthFactorsReply> reply) {
  return reply.has_value() &&
         std::ranges::any_of(
             reply->configured_auth_factors(), [](const auto& factor) {
               return factor.type() == user_data_auth::AUTH_FACTOR_TYPE_PIN;
             });
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

void AuthenticatorChromeOS::CheckIfPinIsAvailable(
    base::OnceCallback<void(bool)> callback) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();

  if (!user) {
    // Post the callback to ensure it runs asynchronously, avoiding the risks
    // of mixing sync and async execution paths.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  user_data_auth::ListAuthFactorsRequest request;

  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(user->GetAccountId());

  ash::UserDataAuthClient::Get()->ListAuthFactors(
      request,
      base::BindOnce(&ContainsPinAuthFactor).Then(std::move(callback)));
}
