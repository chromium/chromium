// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/fingerprint_storage.h"

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/device_service.h"

namespace chromeos {
namespace quick_unlock {

// static
void FingerprintStorage::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kQuickUnlockFingerprintRecord, 0);
}

FingerprintStorage::FingerprintStorage(Profile* profile) : profile_(profile) {
  if (!chromeos::BiodClient::Get())
    return;

  content::GetDeviceService().BindFingerprint(
      fp_service_.BindNewPipeAndPassReceiver());

  const std::string user_id =
      ProfileHelper::Get()->GetUserIdHashFromProfile(profile_);
  // Get actual records to update cached prefs::kQuickUnlockFingerprintRecord.
  fp_service_->GetRecordsForUser(
      user_id, base::BindOnce(&FingerprintStorage::OnGetRecords,
                              weak_factory_.GetWeakPtr()));
}

FingerprintStorage::~FingerprintStorage() {}

bool FingerprintStorage::IsFingerprintAvailable() const {
  return !ExceededUnlockAttempts() && IsFingerprintEnabled(profile_) &&
         HasRecord();
}

bool FingerprintStorage::HasRecord() const {
  return profile_->GetPrefs()->GetInteger(
             prefs::kQuickUnlockFingerprintRecord) != 0;
}

void FingerprintStorage::AddUnlockAttempt() {
  ++unlock_attempt_count_;
}

void FingerprintStorage::ResetUnlockAttemptCount() {
  unlock_attempt_count_ = 0;
}

bool FingerprintStorage::ExceededUnlockAttempts() const {
  return unlock_attempt_count() >= kMaximumUnlockAttempts;
}

void FingerprintStorage::OnGetRecords(
    const base::flat_map<std::string, std::string>& fingerprints_list_mapping) {
  if (!IsFingerprintDisabledByPolicy(profile_->GetPrefs())) {
    profile_->GetPrefs()->SetInteger(prefs::kQuickUnlockFingerprintRecord,
                                     fingerprints_list_mapping.size());
    return;
  }

  for (const auto& it : fingerprints_list_mapping) {
    fp_service_->RemoveRecord(it.first, base::BindOnce([](bool success) {
                                if (success)
                                  return;
                                LOG(ERROR)
                                    << "Failed to remove fingerprint record";
                              }));
  }

  profile_->GetPrefs()->SetInteger(prefs::kQuickUnlockFingerprintRecord, 0);
}

}  // namespace quick_unlock
}  // namespace chromeos
