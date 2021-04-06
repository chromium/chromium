// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chromeos/components/feature_usage/feature_usage_metrics.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

class PrefRegistrySimple;
class Profile;

namespace chromeos {

class FingerprintStorageTestApi;

namespace quick_unlock {

class FingerprintMetricsReporter;
class QuickUnlockStorage;

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
class FingerprintStorage : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  static const int kMaximumUnlockAttempts = 5;

  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit FingerprintStorage(Profile* profile);
  ~FingerprintStorage() final;

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const final;
  bool IsEnabled() const final;

  // Called after a fingerprint unlock attempt to record the result.
  // `num_attempts`:  Only valid when auth success to record number of attempts.
  void RecordFingerprintUnlockResult(FingerprintUnlockResult result);

  // Returns true if fingerprint unlock is currently available.
  // This does not check if strong auth is available.
  bool IsFingerprintAvailable() const;

  // Returns true if the user has fingerprint record registered.
  bool HasRecord() const;

  // Add a fingerprint unlock attempt count.
  void AddUnlockAttempt();

  // Reset the number of unlock attempts to 0.
  void ResetUnlockAttemptCount();

  // Returns true if the user has exceeded fingerprint unlock attempts.
  bool ExceededUnlockAttempts() const;

  int unlock_attempt_count() const { return unlock_attempt_count_; }

 private:
  void OnGetRecords(const base::flat_map<std::string, std::string>&
                        fingerprints_list_mapping);

  friend class chromeos::FingerprintStorageTestApi;
  friend class QuickUnlockStorage;

  Profile* const profile_;
  // Number of fingerprint unlock attempt.
  int unlock_attempt_count_ = 0;

  mojo::Remote<device::mojom::Fingerprint> fp_service_;

  std::unique_ptr<FingerprintMetricsReporter> metrics_reporter_;
  std::unique_ptr<feature_usage::FeatureUsageMetrics>
      feature_usage_metrics_service_;

  base::WeakPtrFactory<FingerprintStorage> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FingerprintStorage);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_
