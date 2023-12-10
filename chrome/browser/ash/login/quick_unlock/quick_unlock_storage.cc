// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/login_types.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace quick_unlock {
namespace {

base::TimeDelta GetStrongAuthTimeout(PrefService* pref_service) {
  PasswordConfirmationFrequency strong_auth_interval =
      static_cast<PasswordConfirmationFrequency>(
          pref_service->GetInteger(prefs::kQuickUnlockTimeout));
  return PasswordConfirmationFrequencyToTimeDelta(strong_auth_interval);
}

}  // namespace

QuickUnlockStorage::QuickUnlockStorage(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {
  fingerprint_storage_ = std::make_unique<FingerprintStorage>(profile);
  pin_storage_prefs_ = std::make_unique<PinStoragePrefs>(profile->GetPrefs());
}

QuickUnlockStorage::~QuickUnlockStorage() {}

void QuickUnlockStorage::SetClockForTesting(base::Clock* test_clock) {
  clock_ = test_clock;
}

void QuickUnlockStorage::MarkStrongAuth() {
  last_strong_auth_ = clock_->Now();
  fingerprint_storage()->ResetUnlockAttemptCount();
  pin_storage_prefs()->ResetUnlockAttemptCount();
}

bool QuickUnlockStorage::HasStrongAuth() const {
  if (last_strong_auth_.is_null())
    return false;
  return clock_->Now() < TimeOfNextStrongAuth();
}

base::Time QuickUnlockStorage::TimeOfNextStrongAuth() const {
  DCHECK(!last_strong_auth_.is_null());
  return last_strong_auth_ + GetStrongAuthTimeout(profile_->GetPrefs());
}

bool QuickUnlockStorage::IsFingerprintAuthenticationAvailable(
    Purpose purpose) const {
  return HasStrongAuth() &&
         fingerprint_storage_->IsFingerprintAvailable(purpose);
}

bool QuickUnlockStorage::IsPinAuthenticationAvailable(Purpose purpose) const {
  return HasStrongAuth() &&
         pin_storage_prefs_->IsPinAuthenticationAvailable(purpose);
}

bool QuickUnlockStorage::TryAuthenticatePin(const Key& key, Purpose purpose) {
  return HasStrongAuth() &&
         pin_storage_prefs()->TryAuthenticatePin(key, purpose);
}

AuthToken* QuickUnlockStorage::GetAuthToken() {
  if (!auth_token_ || !auth_token_->Identifier().has_value())
    return nullptr;
  return auth_token_.get();
}

UserContext* QuickUnlockStorage::GetUserContext(const std::string& auth_token) {
  if (!auth_token_ || auth_token_->Identifier() != auth_token)
    return nullptr;

  return auth_token_->user_context();
}

void QuickUnlockStorage::ReplaceUserContext(
    const std::string& auth_token,
    std::unique_ptr<UserContext> user_context) {
  if (!auth_token_ || auth_token_->Identifier() != auth_token) {
    // See the comment in `AuthToken::ReplaceUserContext` for a situation in
    // which this might happen.
    LOG(WARNING)
        << "Replacement user context is ignored because auth token is gone";
    return;
  }

  auth_token_->ReplaceUserContext(std::move(user_context));
}

FingerprintState QuickUnlockStorage::GetFingerprintState(Purpose purpose) {
  // Fingerprint is not registered for this account.
  if (!fingerprint_storage_->HasRecord())
    return FingerprintState::UNAVAILABLE;

  // This should not happen, but could in theory (see
  // ExceedAttemptsAndBiodRestart test) in the following scenario:
  // -fingerprint is available, user fails to authenticate multiple times
  // -biod restarts and gives a different (although positive) number of records
  // The change in the number of records would trigger a fingerprint state
  // update for the primary user.
  if (fingerprint_storage_->ExceededUnlockAttempts())
    return FingerprintState::DISABLED_FROM_ATTEMPTS;

  // It has been too long since the last authentication.
  if (!HasStrongAuth())
    return FingerprintState::DISABLED_FROM_TIMEOUT;

  // Auth is available.
  if (IsFingerprintAuthenticationAvailable(purpose))
    return FingerprintState::AVAILABLE_DEFAULT;

  // Default to unavailabe.
  return FingerprintState::UNAVAILABLE;
}

void QuickUnlockStorage::Shutdown() {
  fingerprint_storage_.reset();
  pin_storage_prefs_.reset();
}

}  // namespace quick_unlock
}  // namespace ash
