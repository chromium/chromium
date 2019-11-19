// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_cryptohome.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/account_id/account_id.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "crypto/random.h"

namespace chromeos {
namespace quick_unlock {

namespace {

constexpr int kSaltByteSize = 16;

QuickUnlockStorage* GetPrefsBackend(const AccountId& account_id) {
  return QuickUnlockFactory::GetForAccountId(account_id);
}

void PostResponse(PinBackend::BoolCallback result, bool value) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result), value));
}

PinBackend* g_instance_ = nullptr;

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
  std::string salt;
  crypto::RandBytes(base::WriteInto(&salt, kSaltByteSize + 1), kSaltByteSize);
  base::Base64Encode(salt, &salt);
  DCHECK(!salt.empty());
  return salt;
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
  key.Transform(chromeos::Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
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

void PinBackend::MigrateToCryptohome(Profile* profile, const Key& key) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::MigrateToCryptohome, base::Unretained(this),
                       profile, key));
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

  UserContext user_context;
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  user_context.SetAccountId(user->GetAccountId());
  user_context.SetKey(key);
  cryptohome_backend_->SetPin(
      user_context, storage->pin_storage_prefs()->PinSecret(),
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
    cryptohome_backend_->IsPinSetInCryptohome(account_id, std::move(result));
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
    // If |user_context| is null, then the token timed out.
    const UserContext* user_context = storage->GetUserContext(token);
    if (!user_context) {
      PostResponse(std::move(did_set), false);
      return;
    }
    // There may be a pref value if resetting PIN and the device now supports
    // cryptohome-based PIN.
    storage->pin_storage_prefs()->RemovePin();
    cryptohome_backend_->SetPin(*user_context, pin, base::nullopt,
                                std::move(did_set));
  } else {
    storage->pin_storage_prefs()->SetPin(pin);
    storage->MarkStrongAuth();
    PostResponse(std::move(did_set), true);
  }
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

  if (cryptohome_backend_) {
    // If |user_context| is null, then the token timed out.
    const UserContext* user_context = storage->GetUserContext(token);
    if (!user_context) {
      PostResponse(std::move(did_remove), false);
      return;
    }
    cryptohome_backend_->RemovePin(*user_context, std::move(did_remove));
  } else {
    const bool had_pin = storage->pin_storage_prefs()->IsPinSet();
    storage->pin_storage_prefs()->RemovePin();
    PostResponse(std::move(did_remove), had_pin);
  }
}

void PinBackend::CanAuthenticate(const AccountId& account_id,
                                 BoolCallback result) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::CanAuthenticate, base::Unretained(this),
                       account_id, std::move(result)));
    return;
  }

  if (ShouldUseCryptohome(account_id)) {
    cryptohome_backend_->CanAuthenticate(account_id, std::move(result));
  } else {
    QuickUnlockStorage* storage = GetPrefsBackend(account_id);
    PostResponse(
        std::move(result),
        storage && storage->HasStrongAuth() &&
            storage->pin_storage_prefs()->IsPinAuthenticationAvailable());
  }
}

void PinBackend::TryAuthenticate(const AccountId& account_id,
                                 const Key& key,
                                 BoolCallback result) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::TryAuthenticate, base::Unretained(this),
                       account_id, key, std::move(result)));
    return;
  }

  if (ShouldUseCryptohome(account_id)) {
    cryptohome_backend_->TryAuthenticate(account_id, key, std::move(result));
  } else {
    QuickUnlockStorage* storage = GetPrefsBackend(account_id);
    DCHECK(storage);

    if (!storage->HasStrongAuth()) {
      PostResponse(std::move(result), false);
    } else {
      PostResponse(std::move(result),
                   storage->pin_storage_prefs()->TryAuthenticatePin(key));
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

void PinBackend::OnIsCryptohomeBackendSupported(bool is_supported) {
  if (is_supported)
    cryptohome_backend_ = std::make_unique<PinStorageCryptohome>();
  resolving_backend_ = false;
  for (auto& callback : on_cryptohome_support_received_)
    std::move(callback).Run();
  on_cryptohome_support_received_.clear();
}

void PinBackend::OnPinMigrationAttemptComplete(Profile* profile, bool success) {
  if (success) {
    QuickUnlockStorage* storage = QuickUnlockFactory::GetForProfile(profile);
    storage->pin_storage_prefs()->RemovePin();
  }

  scoped_keep_alive_.reset();
}

}  // namespace quick_unlock
}  // namespace chromeos
