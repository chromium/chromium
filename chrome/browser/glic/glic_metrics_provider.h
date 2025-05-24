// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_METRICS_PROVIDER_H_
#define CHROME_BROWSER_GLIC_GLIC_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace glic {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(TieredRolloutEnablementStatus)
enum class GlicTieredRolloutEnablementStatus {
  kAllProfilesEnabled = 0,
  kSomeProfilesEnabled = 1,
  kNoProfilesEnabled = 2,

  kMaxValue = kNoProfilesEnabled,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:TieredRolloutEnablementStatus)

class GlicMetricsProvider : public metrics::MetricsProvider {
 public:
  GlicMetricsProvider();
  ~GlicMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_METRICS_PROVIDER_H_
