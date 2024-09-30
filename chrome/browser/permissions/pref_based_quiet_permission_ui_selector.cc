// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/pref_based_quiet_permission_ui_selector.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"

PrefBasedQuietPermissionUiSelector::PrefBasedQuietPermissionUiSelector(
    Profile* profile)
    : profile_(profile) {}

PrefBasedQuietPermissionUiSelector::~PrefBasedQuietPermissionUiSelector() =
    default;

void PrefBasedQuietPermissionUiSelector::SelectUiToUse(
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  const ContentSettingsType content_settings_type =
      request->GetContentSettingsType();
  if (content_settings_type == ContentSettingsType::NOTIFICATIONS &&
      profile_->GetPrefs()->GetBoolean(
          prefs::kEnableQuietNotificationPermissionUi)) {
    std::move(callback).Run(
        Decision(QuietUiReason::kEnabledInPrefs, Decision::ShowNoWarning()));
    return;
  }
  if (content_settings_type == ContentSettingsType::GEOLOCATION &&
      profile_->GetPrefs()->GetBoolean(
          prefs::kEnableQuietGeolocationPermissionUi)) {
    std::move(callback).Run(
        Decision(QuietUiReason::kEnabledInPrefs, Decision::ShowNoWarning()));
    return;
  }
  std::move(callback).Run(Decision::UseNormalUiAndShowNoWarning());
}

void PrefBasedQuietPermissionUiSelector::Cancel() {}

bool PrefBasedQuietPermissionUiSelector::IsPermissionRequestSupported(
    permissions::RequestType request_type) {
  if (request_type == permissions::RequestType::kNotifications) {
    return true;
  } else if (request_type == permissions::RequestType::kGeolocation) {
    return true;
  } else {
    return false;
  }
}
