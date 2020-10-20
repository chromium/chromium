// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FAMILY_USER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_FAMILY_USER_METRICS_PROVIDER_H_

#include "base/optional.h"
#include "base/scoped_observer.h"
#include "components/metrics/metrics_provider.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace metrics {
class ChromeUserMetricsExtension;
}  // namespace metrics

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
  enum class LogSegment {
    // User does not fall into any of the below categories. For example, this
    // bucket includes regular users.
    kOther = 0,
    // Supervised primary account with no secondary accounts.
    kSupervisedUser = 1,
    // Supervised primary account with EDU secondary account. If the primary
    // account is supervised, then the secondary account must be EDU if one
    // exists.
    kSupervisedStudent = 2,
    // K-12 EDU primary account on an unmanaged device, regardless of the
    // secondary account.
    kStudentAtHome = 3,
    // Add future entries above this comment, in sync with
    // "FamilyUserLogSegment" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kStudentAtHome
  };

  FamilyUserMetricsProvider();
  FamilyUserMetricsProvider(const FamilyUserMetricsProvider&) = delete;
  FamilyUserMetricsProvider& operator=(const FamilyUserMetricsProvider&) =
      delete;
  ~FamilyUserMetricsProvider() override;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto_unused) override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  static const char* GetHistogramNameForTesting();

 private:
  // The only way the |log_segment_| can change during a ChromeOS session is if
  // a child user adds an EDU secondary account. Since this action doesn't
  // happen often, cache the log segment.
  base::Optional<LogSegment> log_segment_;

  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_;
};

#endif  // CHROME_BROWSER_METRICS_FAMILY_USER_METRICS_PROVIDER_H_
