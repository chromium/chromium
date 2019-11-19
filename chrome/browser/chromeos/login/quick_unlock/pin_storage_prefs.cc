// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_prefs.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace quick_unlock {

// static
void PinStoragePrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(ash::prefs::kQuickUnlockPinSalt, "");
  registry->RegisterStringPref(prefs::kQuickUnlockPinSecret, "");
}

PinStoragePrefs::PinStoragePrefs(PrefService* pref_service)
    : pref_service_(pref_service) {}

PinStoragePrefs::~PinStoragePrefs() {}

void PinStoragePrefs::AddUnlockAttempt() {
  ++unlock_attempt_count_;
}

void PinStoragePrefs::ResetUnlockAttemptCount() {
  unlock_attempt_count_ = 0;
}

bool PinStoragePrefs::IsPinSet() const {
  return !PinSalt().empty() && !PinSecret().empty();
}

void PinStoragePrefs::SetPin(const std::string& pin) {
  const std::string salt = PinBackend::ComputeSalt();
  const std::string secret =
      PinBackend::ComputeSecret(pin, salt, Key::KEY_TYPE_PASSWORD_PLAIN);

  pref_service_->SetString(ash::prefs::kQuickUnlockPinSalt, salt);
  pref_service_->SetString(prefs::kQuickUnlockPinSecret, secret);
}

void PinStoragePrefs::RemovePin() {
  pref_service_->SetString(ash::prefs::kQuickUnlockPinSalt, "");
  pref_service_->SetString(prefs::kQuickUnlockPinSecret, "");
}

std::string PinStoragePrefs::PinSalt() const {
  return pref_service_->GetString(ash::prefs::kQuickUnlockPinSalt);
}

std::string PinStoragePrefs::PinSecret() const {
  return pref_service_->GetString(prefs::kQuickUnlockPinSecret);
}

bool PinStoragePrefs::IsPinAuthenticationAvailable() const {
  const bool exceeded_unlock_attempts =
      unlock_attempt_count() >= kMaximumUnlockAttempts;

  return IsPinEnabled(pref_service_) && !IsPinDisabledByPolicy(pref_service_) &&
         IsPinSet() && !exceeded_unlock_attempts;
}

bool PinStoragePrefs::TryAuthenticatePin(const Key& key) {
  if (!IsPinAuthenticationAvailable())
    return false;

  AddUnlockAttempt();
  return PinBackend::ComputeSecret(key.GetSecret(), PinSalt(),
                                   key.GetKeyType()) == PinSecret();
}

}  // namespace quick_unlock
}  // namespace chromeos
