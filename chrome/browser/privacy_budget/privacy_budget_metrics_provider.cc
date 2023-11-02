// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_metrics_provider.h"

#include "base/check.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"

PrivacyBudgetMetricsProvider::PrivacyBudgetMetricsProvider(
    IdentifiabilityStudyState* study_state)
    : study_state_(study_state) {
  DCHECK(study_state_);
}

void PrivacyBudgetMetricsProvider::OnClientStateCleared() {
  study_state_->ResetPersistedState();
}
