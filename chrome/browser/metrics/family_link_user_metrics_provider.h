// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Categorizes the primary account of the active user profile into a FamilyLink
// supervision type to segment the Chrome user population.
class FamilyLinkUserMetricsProvider : public metrics::MetricsProvider {
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
    // Profile contains users with multiple different supervision status
    // used only when ExtendFamilyLinkUserLogSegmentToAllPlatforms flag is
    // enabled
    kMixedProfile = 3,
    // Add future entries above this comment, in sync with
    // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kMixedProfile
  };

  FamilyLinkUserMetricsProvider() = default;
  FamilyLinkUserMetricsProvider(const FamilyLinkUserMetricsProvider&) = delete;
  FamilyLinkUserMetricsProvider& operator=(
      const FamilyLinkUserMetricsProvider&) = delete;
  ~FamilyLinkUserMetricsProvider() override;

  // metrics::MetricsProvider:
  bool ProvideHistograms() override;

  static const char* GetHistogramNameForTesting();

  // Used to skip the check for active browsers in ProvideHistograms() while
  // testing
  bool skip_active_browser_count_for_unittesting_ = false;

 private:
  absl::optional<LogSegment> SupervisionStatusOfProfile(
      const AccountInfo& account_info);
};

#endif  // CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
