// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/auth/legacy_fingerprint_engine.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/biod/biod_client.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

namespace ash {
namespace quick_unlock {
namespace {

constexpr char kFingerprintUMAFeatureName[] = "Fingerprint";

}

// static
void FingerprintStorage::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kQuickUnlockFingerprintRecord, 0);
}

FingerprintStorage::FingerprintStorage(Profile* profile)
    : profile_(profile),
      auth_performer_(UserDataAuthClient::Get()),
      legacy_fingerprint_engine_(&auth_performer_) {
  if (!BiodClient::Get()) {
    // Could be nullptr in tests.
    return;
  }

  content::GetDeviceService().BindFingerprint(
      fp_service_.BindNewPipeAndPassReceiver());

  fp_service_->AddFingerprintObserver(
      fingerprint_observer_receiver_.BindNewPipeAndPassRemote());

  GetRecordsForUser();

  feature_usage_metrics_service_ =
      std::make_unique<feature_usage::FeatureUsageMetrics>(
          kFingerprintUMAFeatureName, this);

  fingerprint_power_button_race_detector_ =
      IsFingerprintSupported()
          ? std::make_unique<FingerprintPowerButtonRaceDetector>(
                chromeos::PowerManagerClient::Get())
          : nullptr;
}

FingerprintStorage::~FingerprintStorage() = default;

void FingerprintStorage::GetRecordsForUser() {
  const std::string user_id =
      ProfileHelper::Get()->GetUserIdHashFromProfile(profile_);
  fp_service_->GetRecordsForUser(
      user_id, base::BindOnce(&FingerprintStorage::OnGetRecords,
                              weak_factory_.GetWeakPtr()));
}

bool FingerprintStorage::IsEligible() const {
  return IsFingerprintSupported();
}

std::optional<bool> FingerprintStorage::IsAccessible() const {
  return legacy_fingerprint_engine_.IsFingerprintEnabled(
      *profile_->GetPrefs(), LegacyFingerprintEngine::Purpose::kAny);
}

bool FingerprintStorage::IsEnabled() const {
  return IsAccessible() && HasRecord();
}

void FingerprintStorage::RecordFingerprintUnlockResult(
    FingerprintUnlockResult result) {
  base::UmaHistogramEnumeration("Fingerprint.Unlock.Result", result);

  const bool success = (result == FingerprintUnlockResult::kSuccess);
  base::UmaHistogramBoolean("Fingerprint.Unlock.AuthSuccessful", success);
  if (success) {
    base::UmaHistogramCounts100("Fingerprint.Unlock.AttemptsCountBeforeSuccess",
                                unlock_attempt_count());
    base::UmaHistogramCounts100(
        "Fingerprint.Unlock.RecentAttemptsCountBeforeSuccess",
        GetRecentUnlockAttemptCount(base::TimeTicks::Now()));
  }
  feature_usage_metrics_service_->RecordUsage(success);
}

bool FingerprintStorage::IsFingerprintAvailable(Purpose purpose) const {
  return !ExceededUnlockAttempts() &&
         legacy_fingerprint_engine_.IsFingerprintEnabled(
             *profile_->GetPrefs(),
             legacy_fingerprint_engine_.FromQuickUnlockPurpose(purpose)) &&
         HasRecord();
}

bool FingerprintStorage::HasRecord() const {
  return profile_->GetPrefs()->GetInteger(
             prefs::kQuickUnlockFingerprintRecord) != 0;
}

void FingerprintStorage::AddUnlockAttempt(base::TimeTicks timestamp) {
  DCHECK_GE(timestamp, last_unlock_attempt_timestamp_);

  ++unlock_attempt_count_;
  if (timestamp - last_unlock_attempt_timestamp_ < kRecentUnlockAttemptsDelta)
    ++recent_unlock_attempt_count_;
  else
    recent_unlock_attempt_count_ = 1;
  last_unlock_attempt_timestamp_ = timestamp;
}

void FingerprintStorage::ResetUnlockAttemptCount() {
  unlock_attempt_count_ = 0;
  recent_unlock_attempt_count_ = 0;
}

bool FingerprintStorage::ExceededUnlockAttempts() const {
  return unlock_attempt_count() >= kMaximumUnlockAttempts;
}

int FingerprintStorage::GetRecentUnlockAttemptCount(base::TimeTicks timestamp) {
  DCHECK_GE(timestamp, last_unlock_attempt_timestamp_);

  if (timestamp - last_unlock_attempt_timestamp_ < kRecentUnlockAttemptsDelta)
    return recent_unlock_attempt_count_;
  else
    return 0;
}

void FingerprintStorage::OnRestarted() {
  LOG(WARNING) << "Biod restarted";
}

void FingerprintStorage::OnStatusChanged(
    device::mojom::BiometricsManagerStatus status) {
  LOG(WARNING) << "Biod status changed";
  if (status == device::mojom::BiometricsManagerStatus::INITIALIZED) {
    GetRecordsForUser();
  } else {
    LOG(ERROR) << "FingerprintStorage StatusChanged to an unknown state"
               << static_cast<int>(status);
  }
}

void FingerprintStorage::OnEnrollScanDone(device::mojom::ScanResult scan_result,
                                          bool is_complete,
                                          int32_t percent_complete) {
  base::UmaHistogramEnumeration("Fingerprint.Enroll.ScanResult", scan_result);
}

void FingerprintStorage::OnAuthScanDone(
    const device::mojom::FingerprintMessagePtr msg,
    const base::flat_map<std::string, std::vector<std::string>>& matches) {
  // could be null in tests
  if (fingerprint_power_button_race_detector_ != nullptr) {
    fingerprint_power_button_race_detector_->FingerprintScanReceived(
        base::TimeTicks::Now());
  }
  switch (msg->which()) {
    case device::mojom::FingerprintMessage::Tag::kScanResult:
      base::UmaHistogramEnumeration("Fingerprint.Auth.ScanResult",
                                    msg->get_scan_result());
      return;
    case device::mojom::FingerprintMessage::Tag::kFingerprintError:
      base::UmaHistogramEnumeration("Fingerprint.Auth.Error",
                                    msg->get_fingerprint_error());
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void FingerprintStorage::OnSessionFailed() {}

void FingerprintStorage::OnGetRecords(
    const base::flat_map<std::string, std::string>& fingerprints_list_mapping,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Get Records failure";
    return;
  }
  if (!legacy_fingerprint_engine_.IsFingerprintDisabledByPolicy(
          *profile_->GetPrefs(), LegacyFingerprintEngine::Purpose::kAny)) {
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
}  // namespace ash
