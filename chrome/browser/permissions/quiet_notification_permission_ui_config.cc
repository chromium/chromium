// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"

// static
const char QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation[] =
    "enable_adaptive_activation";

// static
const char
    QuietNotificationPermissionUiConfig::kEnableAdaptiveActivationDryRun[] =
        "enable_adaptive_activation_dry_run";

// static
const char QuietNotificationPermissionUiConfig::
    kAdaptiveActivationActionWindowSizeInDays[] =
        "adaptive_activation_windows_size_in_days";

// static
const char QuietNotificationPermissionUiConfig::kEnableCrowdDenyTriggering[] =
    "enable_crowd_deny_triggering";

// static
const char QuietNotificationPermissionUiConfig::kCrowdDenyHoldBackChance[] =
    "crowd_deny_hold_back_chance";

// static
const char
    QuietNotificationPermissionUiConfig::kEnableAbusiveRequestBlocking[] =
        "enable_abusive_request_triggering";

// static
const char QuietNotificationPermissionUiConfig::kEnableAbusiveRequestWarning[] =
    "enable_abusive_request_warning";

// static
const char QuietNotificationPermissionUiConfig::
    kEnableAbusiveContentTriggeredRequestBlocking[] =
        "enable_abusive_content_triggering";

// static
const char QuietNotificationPermissionUiConfig::
    kEnableAbusiveContentTriggeredRequestWarning[] =
        "enable_abusive_content_warning";

// static
const char QuietNotificationPermissionUiConfig::kMiniInfobarExpandLinkText[] =
    "mini_infobar_expand_link_text";

// static
bool QuietNotificationPermissionUiConfig::IsAdaptiveActivationEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableAdaptiveActivation,
      false /* default */);
}

// static
bool QuietNotificationPermissionUiConfig::IsAdaptiveActivationDryRunEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableAdaptiveActivationDryRun,
      false /* default */);
}

// static
base::TimeDelta
QuietNotificationPermissionUiConfig::GetAdaptiveActivationWindowSize() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return base::Days(90);

  return base::Days(base::GetFieldTrialParamByFeatureAsInt(
      features::kQuietNotificationPrompts,
      kAdaptiveActivationActionWindowSizeInDays, 90 /* default */));
}

// static
bool QuietNotificationPermissionUiConfig::IsCrowdDenyTriggeringEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableCrowdDenyTriggering,
      true /* default */);
}

// static
double QuietNotificationPermissionUiConfig::GetCrowdDenyHoldBackChance() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      features::kQuietNotificationPrompts, kCrowdDenyHoldBackChance, 0.3);
}

// static
QuietNotificationPermissionUiConfig::InfobarLinkTextVariation
QuietNotificationPermissionUiConfig::GetMiniInfobarExpandLinkText() {
  return base::GetFieldTrialParamByFeatureAsInt(
             features::kQuietNotificationPrompts, kMiniInfobarExpandLinkText, 0)
             ? InfobarLinkTextVariation::kManage
             : InfobarLinkTextVariation::kDetails;
}

// static
bool QuietNotificationPermissionUiConfig::IsAbusiveRequestBlockingEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableAbusiveRequestBlocking,
      true /* default */);
}

// static
bool QuietNotificationPermissionUiConfig::IsAbusiveRequestWarningEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableAbusiveRequestWarning,
      true /* default */);
}

// static
bool QuietNotificationPermissionUiConfig::
    IsAbusiveContentTriggeredRequestBlockingEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts,
      kEnableAbusiveContentTriggeredRequestBlocking, true /* default */);
}

// static
bool QuietNotificationPermissionUiConfig::
    IsAbusiveContentTriggeredRequestWarningEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts,
      kEnableAbusiveContentTriggeredRequestWarning, true /* default */);
}

// static
bool QuietNotificationPermissionUiConfig::
    IsDisruptiveBehaviorRequestBlockingEnabled() {
  return base::FeatureList::IsEnabled(features::kQuietNotificationPrompts) &&
         base::FeatureList::IsEnabled(
             features::kDisruptiveNotificationPermissionRevocation);
}
