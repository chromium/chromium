// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/quiet_permission_prompt_model_android.h"

#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
using PrimaryButtonBehavior =
    QuietPermissionPromptModelAndroid::PrimaryButtonBehavior;
using SecondaryButtonBehavior =
    QuietPermissionPromptModelAndroid::SecondaryButtonBehavior;

std::u16string GetPermissionBlockedTitle(
    ContentSettingsType content_settings_type) {
  switch (content_settings_type) {
    case ContentSettingsType::NOTIFICATIONS:
      return l10n_util::GetStringUTF16(
          IDS_NOTIFICATION_QUIET_PERMISSION_INFOBAR_TITLE);
    case ContentSettingsType::GEOLOCATION:
      return l10n_util::GetStringUTF16(
          IDS_LOCATION_QUIET_PERMISSION_MESSAGE_UI_TITLE);
    default:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

std::u16string GetGeolocationBlockedUIDescription(QuietUiReason reason) {
  switch (reason) {
    case QuietUiReason::kEnabledInPrefs:
      return l10n_util::GetStringUTF16(
          IDS_LOCATION_QUIET_PERMISSION_MESSAGE_UI);
    case QuietUiReason::kServicePredictedVeryUnlikelyGrant:
    case QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant:
      return l10n_util::GetStringUTF16(
          IDS_LOCATION_QUIET_PERMISSION_MESSAGE_UI_PREDICTION_SERVICE);
    default:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

std::u16string GetNotificationBlockedUIDescription(QuietUiReason reason) {
  switch (reason) {
    case QuietUiReason::kEnabledInPrefs:
      return l10n_util::GetStringUTF16(
          IDS_NOTIFICATION_QUIET_PERMISSION_PROMPT_MESSAGE);
    case QuietUiReason::kTriggeredByCrowdDeny:
      return l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_CROWD_DENY_DESCRIPTION);
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
    case QuietUiReason::kTriggeredDueToAbusiveContent:
      return l10n_util::GetStringUTF16(
          IDS_NOTIFICATION_QUIET_PERMISSION_INFOBAR_ABUSIVE_MESSAGE);
    case QuietUiReason::kServicePredictedVeryUnlikelyGrant:
    case QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant:
      return l10n_util::GetStringUTF16(
          IDS_NOTIFICATION_QUIET_PERMISSION_INFOBAR_PREDICTION_SERVICE_MESSAGE);
    case QuietUiReason::kTriggeredDueToDisruptiveBehavior:
      return l10n_util::GetStringUTF16(
          IDS_NOTIFICATION_QUIET_PERMISSION_INFOBAR_DISRUPTIVE_MESSAGE);
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

std::u16string GetPermissionBlockedUIDescription(
    QuietUiReason reason,
    ContentSettingsType content_settings_type) {
  switch (content_settings_type) {
    case ContentSettingsType::NOTIFICATIONS:
      return GetNotificationBlockedUIDescription(reason);
    case ContentSettingsType::GEOLOCATION:
      return GetGeolocationBlockedUIDescription(reason);
    default:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

}  // namespace

QuietPermissionPromptModelAndroid::QuietPermissionPromptModelAndroid() =
    default;
QuietPermissionPromptModelAndroid::~QuietPermissionPromptModelAndroid() =
    default;
QuietPermissionPromptModelAndroid::QuietPermissionPromptModelAndroid(
    const QuietPermissionPromptModelAndroid& other) = default;

QuietPermissionPromptModelAndroid GetQuietPermissionPromptModel(
    permissions::PermissionUiSelector::QuietUiReason reason,
    ContentSettingsType content_settings_type) {
  QuietPermissionPromptModelAndroid model;

  model.title = GetPermissionBlockedTitle(content_settings_type);
  model.description =
      GetPermissionBlockedUIDescription(reason, content_settings_type);

  switch (reason) {
    case QuietUiReason::kEnabledInPrefs:
    case QuietUiReason::kServicePredictedVeryUnlikelyGrant:
    case QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant:
    case QuietUiReason::kTriggeredByCrowdDeny:
      model.primary_button_label = l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ALLOW_BUTTON);
      model.primary_button_behavior = PrimaryButtonBehavior::kAllowForThisSite;
      model.secondary_button_label =
          l10n_util::GetStringUTF16(IDS_NOTIFICATION_BUTTON_MANAGE);
      model.secondary_button_behavior = SecondaryButtonBehavior::kShowSettings;
      break;
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
    case QuietUiReason::kTriggeredDueToAbusiveContent:
    case QuietUiReason::kTriggeredDueToDisruptiveBehavior:
      model.primary_button_label = l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_CONTINUE_BLOCKING_BUTTON);
      model.primary_button_behavior = PrimaryButtonBehavior::kContinueBlocking;
      model.secondary_button_label = l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_COMPACT_ALLOW_BUTTON);
      model.secondary_button_behavior =
          SecondaryButtonBehavior::kAllowForThisSite;
      break;
  }

  if (reason == QuietUiReason::kTriggeredDueToAbusiveRequests ||
      reason == QuietUiReason::kTriggeredDueToAbusiveContent ||
      reason == QuietUiReason::kTriggeredDueToDisruptiveBehavior) {
    model.learn_more_text = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  }

  return model;
}

GURL GetNotificationBlockedLearnMoreUrl() {
  return GURL(u"https://support.google.com/chrome/answer/3220216");
}
