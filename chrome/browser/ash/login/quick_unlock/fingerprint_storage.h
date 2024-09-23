// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/auth/legacy_fingerprint_engine.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_power_button_race_detector.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

class PrefRegistrySimple;
class Profile;

namespace ash {
namespace quick_unlock {

// The result of fingerprint auth attempt on the lock screen. These values are
// persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class FingerprintUnlockResult {
  kSuccess = 0,
  kFingerprintUnavailable = 1,
  kAuthTemporarilyDisabled = 2,
  kMatchFailed = 3,
  kMatchNotForPrimaryUser = 4,
  kMaxValue = kMatchNotForPrimaryUser,
};

// `FingerprintStorage` manages fingerprint user preferences. Keeps them in sync
// with the actual fingerprint records state. The class also reports fingerprint
// metrics.
class FingerprintStorage final
    : public feature_usage::FeatureUsageMetrics::Delegate,
      public device::mojom::FingerprintObserver {
 public:
  static const int kMaximumUnlockAttempts = 5;
  static constexpr base::TimeDelta kRecentUnlockAttemptsDelta =
      base::Seconds(3);
  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit FingerprintStorage(Profile* profile);

  FingerprintStorage(const FingerprintStorage&) = delete;
  FingerprintStorage& operator=(const FingerprintStorage&) = delete;

  ~FingerprintStorage() override;

  // Get actual records to update cached prefs::kQuickUnlockFingerprintRecord.
  void GetRecordsForUser();

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  std::optional<bool> IsAccessible() const override;
  bool IsEnabled() const override;

  // Called after a fingerprint unlock attempt to record the result.
  // `num_attempts`:  Only valid when auth success to record number of attempts.
  void RecordFingerprintUnlockResult(FingerprintUnlockResult result);

  // Returns true if fingerprint unlock is currently available to be used for
  // the specified purpose. When purpose is kAny, it checks if any purpose is
  // enabled. This does not check if strong auth is available.
  bool IsFingerprintAvailable(Purpose purpose) const;

  // Returns true if the user has fingerprint record registered.
  bool HasRecord() const;

  // Add a fingerprint unlock attempt count that happened at timestamp.
  void AddUnlockAttempt(base::TimeTicks timestamp);

  // Reset the number of unlock attempts to 0.
  void ResetUnlockAttemptCount();

  // Returns true if the user has exceeded fingerprint unlock attempts.
  bool ExceededUnlockAttempts() const;

  // Returns the number of unlock attempts made before success, regardless of
  // when they happened in time.
  int unlock_attempt_count() const { return unlock_attempt_count_; }

  // Returns the number of recent unlock attempts made before success.
  // Recent attempts are defined as happening within
  // `kRecentUnlockAttemptsDelta` from each others.
  int GetRecentUnlockAttemptCount(base::TimeTicks timestamp);

  // device::mojom::FingerprintObserver:
  void OnRestarted() override;
  void OnStatusChanged(device::mojom::BiometricsManagerStatus status) override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool is_complete,
                        int32_t percent_complete) override;
  void OnAuthScanDone(
      const device::mojom::FingerprintMessagePtr msg,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override;

 private:
  void OnGetRecords(
      const base::flat_map<std::string, std::string>& fingerprints_list_mapping,
      bool success);

  // GetRecordsForUser logic
  void GetRecordsForUserInternal();

  // On GetRecordsForUser failure, retries to retrieve the records
  void RetryGetRecords();

  friend class FingerprintStorageTestApi;
  friend class QuickUnlockStorage;

  const raw_ptr<Profile> profile_;
  // Number of fingerprint unlock attempts.
  int unlock_attempt_count_ = 0;

  // Number of recent fingerprint unlock attempts, i.e. attempts happening
  // within 3 seconds from each others.
  int recent_unlock_attempt_count_ = 0;

  // Timestamps of the last fingerprint unlock attempt.
  base::TimeTicks last_unlock_attempt_timestamp_ = base::TimeTicks::UnixEpoch();

  mojo::Remote<device::mojom::Fingerprint> fp_service_;

  mojo::Receiver<device::mojom::FingerprintObserver>
      fingerprint_observer_receiver_{this};

  std::unique_ptr<feature_usage::FeatureUsageMetrics>
      feature_usage_metrics_service_;

  std::unique_ptr<FingerprintPowerButtonRaceDetector>
      fingerprint_power_button_race_detector_;

  AuthPerformer auth_performer_;

  LegacyFingerprintEngine legacy_fingerprint_engine_;

  base::WeakPtrFactory<FingerprintStorage> weak_factory_{this};
};

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_
