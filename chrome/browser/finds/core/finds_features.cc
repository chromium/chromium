// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_features.h"

namespace finds::features {

BASE_FEATURE(kChromeFinds, base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/493992939): Remove and merge usages under the main flag.
BASE_FEATURE(kChromeFindsInternals, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kModelExecutionCooldownDurationInDays{
    &kChromeFinds, "model_execution_cooldown_duration_in_days",
    /*default_value=*/7};

constexpr base::FeatureParam<int> kThemeCooldownDurationInDays{
    &kChromeFinds, "theme_cooldown_duration_in_days", /*default_value=*/28};

constexpr base::FeatureParam<int> kNotificationStartTimeMinutes{
    &kChromeFinds, "finds_notification_schedule_start_time_minutes",
    /*default_value=*/120};

constexpr base::FeatureParam<int> kNotificationWindowTimeMinutes{
    &kChromeFinds, "finds_notification_schedule_window_time_minutes",
    /*default_value=*/120};

constexpr base::FeatureParam<int> kFindsOptInPromoMaxInteractedCount{
    &kChromeFinds, "finds_opt_in_promo_max_interacted_count",
    /*default_value=*/2};

constexpr base::FeatureParam<int> kFindsOptInPromoCooldownInDays{
    &kChromeFinds, "finds_opt_in_promo_cooldown_in_days", /*default_value=*/7};

constexpr base::FeatureParam<int> kThemeUrlVisitCountForOptIn{
    &kChromeFinds, "finds_theme_url_visit_count_for_opt_in",
    /*default_value=*/3};

constexpr base::FeatureParam<int> kMaxHistoryEntries{
    &kChromeFinds, "max_history_entries", /*default_value=*/0};

constexpr base::FeatureParam<int> kSRPReturnCountThreshold{
    &kChromeFinds, "srp_return_count_threshold", /*default_value=*/3};

constexpr base::FeatureParam<bool> kEnableSrpReturnCountOptIn{
    &kChromeFinds, "enable_srp_return_count_opt_in", /*default_value=*/true};

constexpr base::FeatureParam<bool> kEnableThemeUrlVisitCountOptIn{
    &kChromeFinds, "enable_theme_url_visit_count_opt_in",
    /*default_value=*/true};

constexpr base::FeatureParam<bool> kBlockModelExecution{
    &kChromeFinds, "block_model_execution", /*default_value=*/false};

}  // namespace finds::features
