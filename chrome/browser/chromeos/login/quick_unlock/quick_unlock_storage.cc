// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/chromeos/login/quick_unlock/auth_token.h"
#include "chrome/browser/chromeos/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
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
  return TimeSinceLastStrongAuth() < GetStrongAuthTimeout(profile_->GetPrefs());
}

base::TimeDelta QuickUnlockStorage::TimeSinceLastStrongAuth() const {
  DCHECK(!last_strong_auth_.is_null());
  return clock_->Now() - last_strong_auth_;
}

base::TimeDelta QuickUnlockStorage::TimeUntilNextStrongAuth() const {
  DCHECK(!last_strong_auth_.is_null());
  return GetStrongAuthTimeout(profile_->GetPrefs()) - TimeSinceLastStrongAuth();
}

bool QuickUnlockStorage::IsFingerprintAuthenticationAvailable() const {
  return HasStrongAuth() && fingerprint_storage_->IsFingerprintAvailable();
}

bool QuickUnlockStorage::IsPinAuthenticationAvailable() const {
  return HasStrongAuth() && pin_storage_prefs_->IsPinAuthenticationAvailable();
}

bool QuickUnlockStorage::TryAuthenticatePin(const Key& key) {
  return HasStrongAuth() && pin_storage_prefs()->TryAuthenticatePin(key);
}

std::string QuickUnlockStorage::CreateAuthToken(
    const chromeos::UserContext& user_context) {
  auth_token_ = std::make_unique<AuthToken>(user_context);
  DCHECK(auth_token_->Identifier().has_value());
  return *auth_token_->Identifier();
}

AuthToken* QuickUnlockStorage::GetAuthToken() {
  if (!auth_token_ || !auth_token_->Identifier().has_value())
    return nullptr;
  return auth_token_.get();
}

const UserContext* QuickUnlockStorage::GetUserContext(
    const std::string& auth_token) {
  if (GetAuthToken() && GetAuthToken()->Identifier() != auth_token)
    return nullptr;
  return auth_token_->user_context();
}

void QuickUnlockStorage::Shutdown() {
  fingerprint_storage_.reset();
  pin_storage_prefs_.reset();
}

}  // namespace quick_unlock
}  // namespace chromeos
