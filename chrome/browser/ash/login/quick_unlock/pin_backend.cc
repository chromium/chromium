// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_cryptohome.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/account_id/account_id.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "crypto/random.h"

namespace ash {
namespace quick_unlock {
namespace {

constexpr int kSaltByteSize = 16;

// PIN auto submit backfill only for 6 digits PINs.
constexpr int kPinAutosubmitBackfillLength = 6;
constexpr int kPinAutosubmitMaxPinLength = 12;

QuickUnlockStorage* GetPrefsBackend(const AccountId& account_id) {
  return QuickUnlockFactory::GetForAccountId(account_id);
}

void PostResponse(PinBackend::BoolCallback result, bool value) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result), value));
}

void PostResponse(PinBackend::AvailabilityCallback result,
                  bool enabled,
                  cryptohome::PinLockAvailability available_at) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result), enabled, available_at));
}

void PostResponse(AuthOperationCallback result,
                  std::unique_ptr<UserContext> user_context,
                  bool success) {
  std::optional<AuthenticationError> error = std::nullopt;
  if (!success) {
    error = std::make_optional<AuthenticationError>(AuthFailure::UNLOCK_FAILED);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result), std::move(user_context),
                                std::move(error)));
}

PinBackend* g_instance_ = nullptr;

// UMA Metrics
void RecordUMAHistogram(PinBackend::BackfillEvent event) {
  base::UmaHistogramEnumeration("Ash.Login.PinAutosubmit.Backfill", event);
}

}  // namespace

// static
PinBackend* PinBackend::GetInstance() {
  if (!g_instance_) {
    g_instance_ = new PinBackend();
  }
  return g_instance_;
}

// static
std::string PinBackend::ComputeSalt() {
  // The salt needs to be base64 encoded because the pref service requires a
  // UTF8 string.
  std::array<uint8_t, kSaltByteSize> bytes;
  crypto::RandBytes(bytes);
  return base::Base64Encode(bytes);
}

// static
std::string PinBackend::ComputeSecret(const std::string& pin,
                                      const std::string& salt,
                                      Key::KeyType key_type) {
  DCHECK(key_type == Key::KEY_TYPE_PASSWORD_PLAIN ||
         key_type == Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234);
  DCHECK(!pin.empty());
  DCHECK(!salt.empty());
  if (key_type != Key::KEY_TYPE_PASSWORD_PLAIN)
    return pin;

  Key key(pin);
  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
  return key.GetSecret();
}

// static
void PinBackend::ResetForTesting() {
  delete g_instance_;
  g_instance_ = nullptr;
}

PinBackend::PinBackend() {
  // base::Unretained is safe because the PinBackend instance is never
  // destroyed.
  PinStorageCryptohome::IsSupported(base::BindOnce(
      &PinBackend::OnIsCryptohomeBackendSupported, base::Unretained(this)));
}

PinBackend::~PinBackend() {
  DCHECK(on_cryptohome_support_received_.empty());
}

void PinBackend::HasLoginSupport(BoolCallback result) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::HasLoginSupport, base::Unretained(this),
                       std::move(result)));
    return;
  }

  PostResponse(std::move(result), !!cryptohome_backend_);
}

void PinBackend::MigrateToCryptohome(
    Profile* profile,
    std::unique_ptr<UserContext> user_context) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::MigrateToCryptohome, base::Unretained(this),
                       profile, std::move(user_context)));
    return;
  }

  // No cryptohome support - nothing to migrate.
  if (!cryptohome_backend_)
    return;

  // No pin in prefs - nothing to migrate.
  QuickUnlockStorage* storage = QuickUnlockFactory::GetForProfile(profile);
  if (!storage->pin_storage_prefs()->IsPinSet())
    return;

  // Make sure chrome does not restart while the migration is in progress (ie,
  // to apply new flags).
  scoped_keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::PIN_MIGRATION, KeepAliveRestartOption::DISABLED);

  cryptohome_backend_->SetPin(
      std::move(user_context), storage->pin_storage_prefs()->PinSecret(),
      storage->pin_storage_prefs()->PinSalt(),
      base::BindOnce(&PinBackend::OnPinMigrationAttemptComplete,
                     base::Unretained(this), profile));
}

void PinBackend::IsSet(const AccountId& account_id, BoolCallback result) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::IsSet, base::Unretained(this), account_id,
                       std::move(result)));
    return;
  }

  if (ShouldUseCryptohome(account_id)) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(account_id);
    if (!user) {
      NOTREACHED_IN_MIGRATION() << "IsSet called with invalid user";
      std::move(result).Run(false);
      return;
    }
    auto user_context = std::make_unique<UserContext>(*user);
    cryptohome_backend_->IsPinSetInCryptohome(std::move(user_context),
                                              std::move(result));
  } else {
    QuickUnlockStorage* storage = GetPrefsBackend(account_id);
    PostResponse(std::move(result),
                 storage && storage->pin_storage_prefs()->IsPinSet());
  }
}

void PinBackend::Set(const AccountId& account_id,
                     const std::string& token,
                     const std::string& pin,
                     BoolCallback did_set) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::Set, base::Unretained(this), account_id,
                       token, pin, std::move(did_set)));
    return;
  }

  QuickUnlockStorage* storage = GetPrefsBackend(account_id);
  DCHECK(storage);

  if (cryptohome_backend_) {
    if (!ash::AuthSessionStorage::Get()->IsValid(token)) {
      PostResponse(std::move(did_set), false);
      return;
    }
    ash::AuthSessionStorage::Get()->BorrowAsync(
        FROM_HERE, token,
        base::BindOnce(&PinBackend::SetWithContext, base::Unretained(this),
                       account_id, token, pin, std::move(did_set)));
  } else {
    storage->pin_storage_prefs()->SetPin(pin);
    storage->MarkStrongAuth();
    UpdatePinAutosubmitOnSet(account_id, pin.length());
    PostResponse(std::move(did_set), true);
  }
}

void PinBackend::SetWithContext(const AccountId& account_id,
                                const std::string& token,
                                const std::string& pin,
                                BoolCallback did_set,
                                std::unique_ptr<UserContext> user_context) {
  if (!user_context) {
    PostResponse(std::move(did_set), false);
    return;
  }
  QuickUnlockStorage* storage = GetPrefsBackend(account_id);
  CHECK(storage);
  // There may be a pref value if resetting PIN and the device now supports
  // cryptohome-based PIN.
  storage->pin_storage_prefs()->RemovePin();
  cryptohome_backend_->SetPin(
      std::move(user_context), pin, std::nullopt,
      base::BindOnce(&PinBackend::OnAuthOperation, token, std::move(did_set)));
  UpdatePinAutosubmitOnSet(account_id, pin.length());
}

void PinBackend::SetPinAutoSubmitEnabled(const AccountId& account_id,
                                         const std::string& pin,
                                         const bool enabled,
                                         BoolCallback did_set) {
  // Immediate false if the PIN length isn't supported, or when the feature
  // isdisabled.
  if (pin.length() > kPinAutosubmitMaxPinLength) {
    PostResponse(std::move(did_set), false);
    return;
  }

  // If the preference is not user controllable, the auto submit dialog
  // isn't available in Settings, so we return a failure.
  if (!PrefService(account_id)
           ->IsUserModifiablePreference(::prefs::kPinUnlockAutosubmitEnabled)) {
    PostResponse(std::move(did_set), false);
    return;
  }

  if (!enabled) {
    user_manager::KnownUser known_user(g_browser_process->local_state());
    known_user.SetUserPinLength(account_id, 0);
    PrefService(account_id)
        ->SetBoolean(::prefs::kPinUnlockAutosubmitEnabled, false);
    PostResponse(std::move(did_set), true);
    return;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user) {
    NOTREACHED_IN_MIGRATION() << "IsSet called with invalid user";
    std::move(did_set).Run(false);
    return;
  }
  auto user_context = std::make_unique<UserContext>(*user);
  user_context->SetIsUsingPin(true);

  // Unretained safe because the PinBackend instance is never destroyed.
  TryAuthenticate(
      std::move(user_context), Key(pin), Purpose::kAny,
      base::BindOnce(&PinBackend::OnPinAutosubmitCheckComplete,
                     base::Unretained(this), pin.size(), std::move(did_set)));
}

void PinBackend::Remove(const AccountId& account_id,
                        const std::string& token,
                        BoolCallback did_remove) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::Remove, base::Unretained(this), account_id,
                       token, std::move(did_remove)));
    return;
  }

  QuickUnlockStorage* storage = GetPrefsBackend(account_id);
  DCHECK(storage);

  // Disable PIN auto submit when removing the pin.
  UpdatePinAutosubmitOnRemove(account_id);

  if (cryptohome_backend_) {
    if (!ash::AuthSessionStorage::Get()->IsValid(token)) {
      PostResponse(std::move(did_remove), false);
      return;
    }
    ash::AuthSessionStorage::Get()->BorrowAsync(
        FROM_HERE, token,
        base::BindOnce(&PinBackend::RemoveWithContext, base::Unretained(this),
                       account_id, token, std::move(did_remove)));
  } else {
    const bool had_pin = storage->pin_storage_prefs()->IsPinSet();
    storage->pin_storage_prefs()->RemovePin();
    PostResponse(std::move(did_remove), had_pin);
  }
}

void PinBackend::RemoveWithContext(const AccountId& account_id,
                                   const std::string& token,
                                   BoolCallback did_remove,
                                   std::unique_ptr<UserContext> user_context) {
  cryptohome_backend_->RemovePin(std::make_unique<UserContext>(*user_context),
                                 base::BindOnce(&PinBackend::OnAuthOperation,
                                                token, std::move(did_remove)));
}

void PinBackend::CanAuthenticate(const AccountId& account_id,
                                 Purpose purpose,
                                 AvailabilityCallback result_callback) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::CanAuthenticate, base::Unretained(this),
                       account_id, purpose, std::move(result_callback)));
    return;
  }

  if (ShouldUseCryptohome(account_id)) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(account_id);
    if (!user) {
      NOTREACHED_IN_MIGRATION() << "CanAuthenticate called with invalid user";
      std::move(result_callback).Run(false, std::nullopt);
      return;
    }
    auto user_context = std::make_unique<UserContext>(*user);
    cryptohome_backend_->CanAuthenticate(std::move(user_context), purpose,
                                         std::move(result_callback));
  } else {
    QuickUnlockStorage* storage = GetPrefsBackend(account_id);
    // pref based pin should be immediately available.
    PostResponse(
        std::move(result_callback),
        storage && storage->HasStrongAuth() &&
            storage->pin_storage_prefs()->IsPinAuthenticationAvailable(purpose),
        std::nullopt);
  }
}

void PinBackend::TryAuthenticate(std::unique_ptr<UserContext> user_context,
                                 const Key& key,
                                 Purpose purpose,
                                 AuthOperationCallback result) {
  DCHECK(user_context->GetAuthSessionId().empty());
  DCHECK(user_context->IsUsingPin());
  const AccountId& account_id = user_context->GetAccountId();
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(base::BindOnce(
        &PinBackend::TryAuthenticate, base::Unretained(this),
        std::move(user_context), key, purpose, std::move(result)));
    return;
  }

  if (ShouldUseCryptohome(account_id)) {
    // Safe because the PinBackend instance is never destroyed.
    cryptohome_backend_->TryAuthenticate(
        std::move(user_context), key, purpose,
        base::BindOnce(&PinBackend::OnCryptohomeAuthenticationResponse,
                       base::Unretained(this), key, std::move(result)));
  } else {
    QuickUnlockStorage* storage = GetPrefsBackend(account_id);
    DCHECK(storage);

    if (!storage->HasStrongAuth()) {
      PostResponse(std::move(result), std::move(user_context), false);
    } else {
      const bool auth_success =
          storage->pin_storage_prefs()->TryAuthenticatePin(key, purpose);
      if (auth_success && key.GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
        UpdatePinAutosubmitOnSuccessfulTryAuth(account_id,
                                               key.GetSecret().length());
      }
      PostResponse(std::move(result), std::move(user_context), auth_success);
    }
  }
}

bool PinBackend::ShouldUseCryptohome(const AccountId& account_id) {
  if (!cryptohome_backend_)
    return false;

  // Even if cryptohome is supported, the user may have registered a PIN with
  // the prefs backend from a previous version. If that's the case, we should
  // talk to the prefs backend instead of the cryptohome backend.
  QuickUnlockStorage* storage = GetPrefsBackend(account_id);
  return !storage || !storage->pin_storage_prefs()->IsPinSet();
}

int PinBackend::GetExposedPinLength(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());

  // Clear the pin length in local state if auto-submit got disabled, for
  // example, via policy. Disabling auto submit through Settings clears it
  // immediately.
  if (!PrefService(account_id)
           ->GetBoolean(::prefs::kPinUnlockAutosubmitEnabled)) {
    known_user.SetUserPinLength(account_id, 0);
    return 0;
  }

  return known_user.GetUserPinLength(account_id);
}

void PinBackend::OnIsCryptohomeBackendSupported(bool is_supported) {
  if (is_supported)
    cryptohome_backend_ = std::make_unique<PinStorageCryptohome>();
  resolving_backend_ = false;
  for (auto& callback : on_cryptohome_support_received_)
    std::move(callback).Run();
  on_cryptohome_support_received_.clear();
}

void PinBackend::OnPinMigrationAttemptComplete(
    Profile* profile,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (!error.has_value()) {
    QuickUnlockStorage* storage = QuickUnlockFactory::GetForProfile(profile);
    storage->pin_storage_prefs()->RemovePin();
  }

  scoped_keep_alive_.reset();
}

void PinBackend::OnCryptohomeAuthenticationResponse(
    const Key& key,
    AuthOperationCallback result,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  // Regardless of the outcome, discard the session in user_context. This
  // session was only meant to be used for checking the PIN.
  user_context->ResetAuthSessionIds();

  const bool success = !error.has_value();
  const AccountId& account_id = user_context->GetAccountId();

  if (success && key.GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    UpdatePinAutosubmitOnSuccessfulTryAuth(account_id,
                                           key.GetSecret().length());

    // Mark the PIN as strong auth factor if the authentication was successful.
    QuickUnlockStorage* storage = GetPrefsBackend(user_context->GetAccountId());
    DCHECK(storage);
    if (storage) {
      storage->MarkStrongAuth();
    }
  }

  std::move(result).Run(std::move(user_context), std::move(error));
}

void PinBackend::OnPinAutosubmitCheckComplete(
    size_t pin_length,
    BoolCallback result,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  const bool success = !error.has_value();
  const AccountId& account_id = user_context->GetAccountId();
  user_manager::KnownUser known_user(g_browser_process->local_state());
  // Set the exposed PIN to zero, if not successful.
  known_user.SetUserPinLength(account_id, success ? pin_length : 0);
  PrefService(account_id)
      ->SetBoolean(::prefs::kPinUnlockAutosubmitEnabled, success);
  PostResponse(std::move(result), success);
}

PrefService* PinBackend::PrefService(const AccountId& account_id) {
  return ProfileHelper::Get()->GetProfileByAccountId(account_id)->GetPrefs();
}

void PinBackend::UpdatePinAutosubmitOnSet(const AccountId& account_id,
                                          size_t pin_length) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  // A PIN is being set when the auto submit feature is present. This user
  // does not need to be backfilled.
  known_user.PinAutosubmitSetBackfillNotNeeded(account_id);

  const bool autosubmit_enabled =
      PrefService(account_id)
          ->GetBoolean(::prefs::kPinUnlockAutosubmitEnabled) &&
      pin_length <= kPinAutosubmitMaxPinLength;

  // Explicitly set the user pref to false if the PIN is longer than 12 digits
  // so that the toggle on the Settings page remains unchecked. If the user
  // tries to enable the toggle with a long pin an error is shown.
  if (pin_length > kPinAutosubmitMaxPinLength) {
    PrefService(account_id)
        ->SetBoolean(::prefs::kPinUnlockAutosubmitEnabled, false);
  }

  // Expose the true PIN length if enabled
  pin_length = autosubmit_enabled ? pin_length : 0;
  known_user.SetUserPinLength(account_id, pin_length);
}

void PinBackend::UpdatePinAutosubmitOnRemove(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetUserPinLength(account_id, 0);
  PrefService(account_id)->ClearPref(::prefs::kPinUnlockAutosubmitEnabled);
}

void PinBackend::UpdatePinAutosubmitOnSuccessfulTryAuth(
    const AccountId& account_id,
    size_t pin_length) {
  // Backfill the auto submit preference if the PIN that was authenticated was
  // set before the auto submit feature existed.
  PinAutosubmitBackfill(account_id, pin_length);

  const bool autosubmit_enabled =
      PrefService(account_id)
          ->GetBoolean(::prefs::kPinUnlockAutosubmitEnabled) &&
      pin_length <= kPinAutosubmitMaxPinLength;
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (autosubmit_enabled)
    known_user.SetUserPinLength(account_id, pin_length);
}

void PinBackend::PinAutosubmitBackfill(const AccountId& account_id,
                                       size_t pin_length) {
  if (!features::IsPinAutosubmitBackfillFeatureEnabled()) {
    return;
  }

  user_manager::KnownUser known_user(g_browser_process->local_state());
  // Don't backfill if its not necessary & Prevent future backfill attempts.
  if (!known_user.PinAutosubmitIsBackfillNeeded(account_id))
    return;
  known_user.PinAutosubmitSetBackfillNotNeeded(account_id);

  // Dont backfill if there is a user value set for the pref.
  if (PrefService(account_id)
          ->GetUserPrefValue(::prefs::kPinUnlockAutosubmitEnabled) != nullptr)
    return;

  // Disabled if not allowed by policy. Since 'kPinUnlockAutosubmitEnabled'
  // is enabled by default, it is only false when recommended/mandatory by
  // policy.
  if (!PrefService(account_id)
           ->GetBoolean(::prefs::kPinUnlockAutosubmitEnabled)) {
    RecordUMAHistogram(BackfillEvent::kDisabledDueToPolicy);
    PrefService(account_id)
        ->SetBoolean(::prefs::kPinUnlockAutosubmitEnabled, false);
    return;
  }

  // Only enable auto submit for six digits PINs.
  if (pin_length != kPinAutosubmitBackfillLength) {
    RecordUMAHistogram(BackfillEvent::kDisabledDueToPinLength);
    PrefService(account_id)
        ->SetBoolean(::prefs::kPinUnlockAutosubmitEnabled, false);
  } else {
    RecordUMAHistogram(BackfillEvent::kEnabled);
    PrefService(account_id)
        ->SetBoolean(::prefs::kPinUnlockAutosubmitEnabled, true);
  }
}

void PinBackend::OnAuthOperation(std::string auth_token,
                                 BoolCallback callback,
                                 std::unique_ptr<UserContext> user_context,
                                 std::optional<AuthenticationError> error) {
  ash::AuthSessionStorage::Get()->Return(auth_token, std::move(user_context));
  std::move(callback).Run(!error.has_value());
}

}  // namespace quick_unlock
}  // namespace ash
