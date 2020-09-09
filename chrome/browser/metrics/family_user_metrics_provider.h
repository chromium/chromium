// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FAMILY_USER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_FAMILY_USER_METRICS_PROVIDER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/metrics/cached_metrics_profile.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "components/metrics/metrics_provider.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace metrics {
class ChromeUserMetricsExtension;
}  // namespace metrics

// Provides metrics for user types of interest to the Family Experiences team.
// These categories include supervised users, supervised students, students at
// home, and other. This metrics provider is ChromeOS specific.
class FamilyUserMetricsProvider : public metrics::MetricsProvider {
 public:
  // These enum values represent the current user's log segment for the Family
  // Experiences team's metrics.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "FamilyUserLogSegment" in src/tools/metrics/histograms/enums.xml.
  enum class LogSegment {
    // Supervised primary account with no secondary accounts.
    kSupervisedUser = 0,
    // Supervised primary account with K-12 EDU secondary account. If the
    // primary account is supervised, then the secondary account must be K-12
    // EDU if one exists.
    kSupervisedStudent = 1,
    // K-12 EDU primary account on an unmanaged device, regardless of the
    // secondary account.
    kStudentAtHome = 2,
    // User does not fall into any of the above categories.
    kOther = 3,
    // Add future entries above this comment, in sync with
    // "FamilyUserLogSegment" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kOther
  };

  // Family user metrics log segment histogram name.
  static const char kFamilyUserLogSegmentHistogramName[];

  FamilyUserMetricsProvider();
  ~FamilyUserMetricsProvider() override;
  FamilyUserMetricsProvider(const FamilyUserMetricsProvider&) = delete;
  FamilyUserMetricsProvider& operator=(const FamilyUserMetricsProvider&) =
      delete;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto_unused) override;

 private:
  // Callback handler for |AccountManager::GetAccounts|.
  void CheckSecondaryAccountsAndLogSegment(
      const std::vector<chromeos::AccountManager::Account>& accounts);

  // Returns user's segment for metrics logging.
  enterprise_management::PolicyData::MetricsLogSegment GetMetricsLogSegment();

  // Use the first signed-in profile for profile-dependent metrics.
  metrics::CachedMetricsProfile cached_profile_;

  base::WeakPtrFactory<FamilyUserMetricsProvider> weak_factory_{this};
};

#endif  // CHROME_BROWSER_METRICS_FAMILY_USER_METRICS_PROVIDER_H_
