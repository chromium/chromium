// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_

#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/metrics/metrics_provider.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/core/browser/signin_status_metrics_provider_delegate.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {
class ChromeUserMetricsExtension;
}  // namespace metrics

// Categorizes the user into a FamilyLink supervision type to segment the
// Chrome user population.
class FamilyLinkUserMetricsProvider : public metrics::MetricsProvider,
                                      public IdentityManagerFactory::Observer,
                                      public signin::IdentityManager::Observer {
 public:
  // These enum values represent the user's supervision type and how the
  // supervision has been enabled.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
  enum class LogSegment {
    // User is not supervised by FamilyLink.
    kUnsupervised = 0,
    // User that is required to be supervised by FamilyLink due to child account
    // policies (maps to Unicorn and Griffin accounts).
    kSupervisionEnabledByPolicy = 1,
    // User that has chosen to be supervised by FamilyLink (maps to Geller
    // accounts).
    kSupervisionEnabledByUser = 2,
    // Add future entries above this comment, in sync with
    // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kSupervisionEnabledByUser
  };

  FamilyLinkUserMetricsProvider();
  FamilyLinkUserMetricsProvider(const FamilyLinkUserMetricsProvider&) = delete;
  FamilyLinkUserMetricsProvider& operator=(
      const FamilyLinkUserMetricsProvider&) = delete;
  ~FamilyLinkUserMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto_unused) override;

  // IdentityManagerFactoryObserver:
  void IdentityManagerCreated(
      signin::IdentityManager* identity_manager) override;

  // signin::IdentityManager::Observer
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  static const char* GetHistogramNameForTesting();

 private:
  void SetLogSegment(LogSegment log_segment);

  // Used to track the IdentityManagers that this instance is observing so that
  // this instance can be removed as an observer on its destruction.
  base::ScopedMultiSourceObservation<signin::IdentityManager,
                                     signin::IdentityManager::Observer>
      scoped_observations_{this};

  // Used to track the IdentityManagerFactory instance.
  base::ScopedObservation<IdentityManagerFactory,
                          IdentityManagerFactory::Observer>
      scoped_factory_observation_{this};

  // Cache the log segment because it won't change during the session once
  // assigned.
  absl::optional<LogSegment> log_segment_;
};

#endif  // CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
