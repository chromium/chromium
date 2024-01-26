// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FAMILY_USER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_FAMILY_USER_METRICS_PROVIDER_H_

#include <optional>

#include "base/scoped_multi_source_observation.h"
#include "components/metrics/metrics_provider.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

// Categorizes the current user into a family user type for UMA dashboard
// filtering. This metrics provider is ChromeOS specific.
class FamilyUserMetricsProvider
    : public metrics::MetricsProvider,
      public session_manager::SessionManagerObserver,
      public signin::IdentityManager::Observer {
 public:
  // These enum values represent the current user's log segment for the Family
  // Experiences team's metrics.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "FamilyUserLogSegment" in src/tools/metrics/histograms/enums.xml.
  enum class FamilyUserLogSegment {
    // User does not fall into any of the below categories.
    kOther = 0,
    // Supervised primary account with no secondary accounts.
    kSupervisedUser = 1,
    // Supervised primary account with EDU secondary account. If the primary
    // account is supervised, then the secondary account must be EDU if one
    // exists.
    kSupervisedStudent = 2,
    // Kindergarten-12th grade (K-12) EDU primary account on an unmanaged
    // device, regardless of the secondary account.
    kStudentAtHome = 3,
    // Regular unmanaged user on any device, regardless of the secondary
    // account.
    kRegularUser = 4,
    // Add future entries above this comment, in sync with
    // "FamilyUserLogSegment" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kRegularUser
  };

  FamilyUserMetricsProvider();
  FamilyUserMetricsProvider(const FamilyUserMetricsProvider&) = delete;
  FamilyUserMetricsProvider& operator=(const FamilyUserMetricsProvider&) =
      delete;
  ~FamilyUserMetricsProvider() override;

  // MetricsProvider:
  bool ProvideHistograms() override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;

  static const char* GetFamilyUserLogSegmentHistogramNameForTesting();
  static const char* GetNumSecondaryAccountsHistogramNameForTesting();

 private:
  void ObserveIdentityManager(Profile* profile);
  bool IsSupervisedUser(Profile* profile);
  bool IsSupervisedStudent(Profile* profile);

  // The only way the |family_user_log_segment_| can change during a ChromeOS
  // session is if a child user adds or removes an EDU secondary account. Since
  // this action doesn't happen often, cache the log segment.
  std::optional<FamilyUserLogSegment> family_user_log_segment_;
  int num_secondary_accounts_ = -1;

  base::ScopedMultiSourceObservation<signin::IdentityManager,
                                     signin::IdentityManager::Observer>
      identity_manager_observations_{this};
};

#endif  // CHROME_BROWSER_METRICS_FAMILY_USER_METRICS_PROVIDER_H_
