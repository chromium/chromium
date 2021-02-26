// Copyright 2021 The Chromium Authors. All rights reserved.
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

void PrivacyBudgetMetricsProvider::Init() {
  study_state_->InitFromPrefs();
}

void PrivacyBudgetMetricsProvider::OnClientStateCleared() {
  study_state_->ResetClientState();
}
