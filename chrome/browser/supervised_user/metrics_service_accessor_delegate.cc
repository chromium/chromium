// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/metrics_service_accessor_delegate.h"

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

namespace supervised_user {
MetricsServiceAccessorDelegateImpl::MetricsServiceAccessorDelegateImpl() =
    default;
MetricsServiceAccessorDelegateImpl::~MetricsServiceAccessorDelegateImpl() =
    default;

void MetricsServiceAccessorDelegateImpl::RegisterSyntheticFieldTrial(
    std::string_view trial_name,
    std::string_view group_name) {
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      trial_name, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}
}  // namespace supervised_user
