// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"

// Categorizes the primary account of the active user profile into a FamilyLink
// supervision type to segment the Chrome user population.
class FamilyLinkUserMetricsProvider : public metrics::MetricsProvider {
 public:
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
};

#endif  // CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
