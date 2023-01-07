// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/pref_notification_permission_ui_selector.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"

PrefNotificationPermissionUiSelector::PrefNotificationPermissionUiSelector(
    Profile* profile)
    : profile_(profile) {}

PrefNotificationPermissionUiSelector::~PrefNotificationPermissionUiSelector() =
    default;

void PrefNotificationPermissionUiSelector::SelectUiToUse(
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  if (base::FeatureList::IsEnabled(features::kQuietNotificationPrompts) &&
      profile_->GetPrefs()->GetBoolean(
          prefs::kEnableQuietNotificationPermissionUi)) {
    std::move(callback).Run(
        Decision(QuietUiReason::kEnabledInPrefs, Decision::ShowNoWarning()));
    return;
  }

  std::move(callback).Run(Decision::UseNormalUiAndShowNoWarning());
}

void PrefNotificationPermissionUiSelector::Cancel() {}

bool PrefNotificationPermissionUiSelector::IsPermissionRequestSupported(
    permissions::RequestType request_type) {
  return request_type == permissions::RequestType::kNotifications;
}
