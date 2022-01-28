// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {
class ChromeUserMetricsExtension;
}  // namespace metrics

// Categorizes the user into a FamilyLink supervision type to segment the
// Chrome user population.
class FamilyLinkUserMetricsProvider
    : public metrics::MetricsProvider,
      public session_manager::SessionManagerObserver {
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

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

 private:
  void SetLogSegment(LogSegment log_segment);

  // Cache the log segment because it won't change during the session once
  // assigned.
  absl::optional<LogSegment> log_segment_;
};

#endif  // CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
