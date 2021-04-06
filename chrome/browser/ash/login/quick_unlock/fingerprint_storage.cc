// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

namespace chromeos {
namespace quick_unlock {

namespace {

constexpr char kFingerprintUMAFeatureName[] = "Fingerprint";

}

class FingerprintMetricsReporter : public device::mojom::FingerprintObserver {
 public:
  // device::mojom::FingerprintObserver:
  void OnRestarted() override {}
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool is_complete,
                        int32_t percent_complete) override {
    base::UmaHistogramEnumeration("Fingerprint.Enroll.ScanResult", scan_result);
  }
  void OnAuthScanDone(
      device::mojom::ScanResult scan_result,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override {
    base::UmaHistogramEnumeration("Fingerprint.Auth.ScanResult", scan_result);
  }
  void OnSessionFailed() override {}

  mojo::PendingRemote<device::mojom::FingerprintObserver> GetRemote() {
    return fingerprint_observer_receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<device::mojom::FingerprintObserver>
      fingerprint_observer_receiver_{this};
};

// static
void FingerprintStorage::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kQuickUnlockFingerprintRecord, 0);
  feature_usage::FeatureUsageMetrics::RegisterPref(registry,
                                                   kFingerprintUMAFeatureName);
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

  metrics_reporter_ = std::make_unique<FingerprintMetricsReporter>();
  fp_service_->AddFingerprintObserver(metrics_reporter_->GetRemote());
  feature_usage_metrics_service_ =
      std::make_unique<feature_usage::FeatureUsageMetrics>(
          kFingerprintUMAFeatureName, profile_->GetPrefs(), this);
}

FingerprintStorage::~FingerprintStorage() {}

bool FingerprintStorage::IsEligible() const {
  return IsFingerprintSupported();
}

bool FingerprintStorage::IsEnabled() const {
  return IsFingerprintEnabled(profile_) && HasRecord();
}

void FingerprintStorage::RecordFingerprintUnlockResult(
    FingerprintUnlockResult result) {
  base::UmaHistogramEnumeration("Fingerprint.Unlock.Result", result);

  const bool success = (result == FingerprintUnlockResult::kSuccess);
  base::UmaHistogramBoolean("Fingerprint.Unlock.AuthSuccessful", success);
  if (success) {
    base::UmaHistogramCounts100("Fingerprint.Unlock.AttemptsCountBeforeSuccess",
                                unlock_attempt_count());
  }
  feature_usage_metrics_service_->RecordUsage(success);
}

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
