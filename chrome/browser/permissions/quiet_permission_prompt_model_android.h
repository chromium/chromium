// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_QUIET_PERMISSION_PROMPT_MODEL_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_QUIET_PERMISSION_PROMPT_MODEL_ANDROID_H_

#include <string>

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_ui_selector.h"
#include "url/gurl.h"

// Model representing the expected UI labels on different surfaces and behaviors
// on related events.
struct QuietPermissionPromptModelAndroid {
  enum class PrimaryButtonBehavior {
    kAllowForThisSite,
    kContinueBlocking,
  };

  enum class SecondaryButtonBehavior {
    kShowSettings,
    kAllowForThisSite,
  };

  QuietPermissionPromptModelAndroid();
  QuietPermissionPromptModelAndroid(
      const QuietPermissionPromptModelAndroid& other);
  ~QuietPermissionPromptModelAndroid();

  std::u16string title;
  std::u16string description;
  std::u16string primary_button_label;
  std::u16string secondary_button_label;
  std::u16string learn_more_text;
  PrimaryButtonBehavior primary_button_behavior;
  SecondaryButtonBehavior secondary_button_behavior;
};

QuietPermissionPromptModelAndroid GetQuietPermissionPromptModel(
    permissions::PermissionUiSelector::QuietUiReason reason,
    ContentSettingsType content_settings_type);

GURL GetNotificationBlockedLearnMoreUrl();

#endif  // CHROME_BROWSER_PERMISSIONS_QUIET_PERMISSION_PROMPT_MODEL_ANDROID_H_
