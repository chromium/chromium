// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_METRICS_PROVIDER_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_METRICS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "components/metrics/metrics_provider.h"

// PrivacyBudgetMetricsProvider responds to events raised by UkmService and
// updates the associated IdentifiabilityStudyState.
class PrivacyBudgetMetricsProvider : public metrics::MetricsProvider {
 public:
  // Constructs a new `PrivacyBudgetMetricsProvider`. The `study_state` passed
  // in must outlive the constructed object.
  //
  // Since this metrics provider is meant to be used with `UkmService` which
  // owns its metrics providers, the `study_state` must outlive the
  // `UkmService` instance.
  explicit PrivacyBudgetMetricsProvider(IdentifiabilityStudyState* study_state);

  // metrics::MetricsProvider
  void OnClientStateCleared() final;

 private:
  raw_ptr<IdentifiabilityStudyState> study_state_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_METRICS_PROVIDER_H_
