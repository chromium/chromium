// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_features.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"

// Keep in sync with "PermissionFieldTrial.java"
const char kQuietNotificationPromptsUIFlavorParameterName[] = "ui_flavour";

#if defined(OS_ANDROID)
const char kQuietNotificationPromptsQuietNotification[] = "quiet_notification";
const char kQuietNotificationPromptsHeadsUpNotification[] =
    "heads_up_notification";
const char kQuietNotificationPromptsMiniInfobar[] = "mini_infobar";
#else   // OS_ANDROID
const char kQuietNotificationPromptsStaticIcon[] = "static_icon";
const char kQuietNotificationPromptsAnimatedIcon[] = "animated_icon";
#endif  // OS_ANDROID

const char kQuietNotificationPromptsActivationParameterName[] = "activation";
const char kQuietNotificationPromptsActivationNever[] = "never";
const char kQuietNotificationPromptsActivationAdaptive[] = "adaptive";
const char kQuietNotificationPromptsActivationAlways[] = "always";

QuietNotificationsPromptConfig::UIFlavor
QuietNotificationsPromptConfig::UIFlavorToUse() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return UIFlavor::NONE;

  std::string ui_flavor = base::GetFieldTrialParamValueByFeature(
      features::kQuietNotificationPrompts,
      kQuietNotificationPromptsUIFlavorParameterName);

#if defined(OS_ANDROID)
  if (ui_flavor == kQuietNotificationPromptsHeadsUpNotification) {
    return UIFlavor::HEADS_UP_NOTIFICATION;
  } else if (ui_flavor == kQuietNotificationPromptsMiniInfobar) {
    return UIFlavor::MINI_INFOBAR;
  } else {
    return UIFlavor::QUIET_NOTIFICATION;
  }
#else   // OS_ANDROID
  if (ui_flavor == kQuietNotificationPromptsStaticIcon) {
    return UIFlavor::STATIC_ICON;
  } else if (ui_flavor == kQuietNotificationPromptsAnimatedIcon) {
    return UIFlavor::ANIMATED_ICON;
  } else {
    return UIFlavor::STATIC_ICON;
  }
#endif  // !OS_ANDROID
}

// static
QuietNotificationsPromptConfig::Activation
QuietNotificationsPromptConfig::GetActivation() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return Activation::kNever;

  std::string ui_flavor = base::GetFieldTrialParamValueByFeature(
      features::kQuietNotificationPrompts,
      kQuietNotificationPromptsActivationParameterName);
  if (ui_flavor == kQuietNotificationPromptsActivationAlways) {
    return Activation::kAlways;
  } else if (ui_flavor == kQuietNotificationPromptsActivationNever) {
    return Activation::kNever;
  } else if (ui_flavor == kQuietNotificationPromptsActivationAdaptive) {
    return Activation::kAdaptive;
  }
  return Activation::kAdaptive;
}
