// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/multidevice_feature_opt_in_view.h"

#include <memory>
#include <string>

#include "ash/components/multidevice/logging/logging.h"
#include "ash/components/phonehub/multidevice_feature_access_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

using multidevice_setup::mojom::Feature;
using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::LogInterstitialScreenEvent;

namespace {

using PermissionSetupMode = MultideviceFeatureOptInView::PermissionSetupMode;

// URL of the multidevice settings page with the URL parameter that will
// start up the opt-in-flow.
// TODO: Update this URL once the new access setup dialog has been updated
constexpr char kMultideviceSettingsUrl[] =
    "chrome://os-settings/multidevice/"
    "features?showPhonePermissionSetupDialog&mode=%d";

PermissionSetupMode GetPermissionSetupMode(
    phonehub::MultideviceFeatureAccessManager*
        multidevice_feature_access_manager) {
  bool can_request_notification_access =
      multidevice_feature_access_manager->GetNotificationAccessStatus() ==
      phonehub::MultideviceFeatureAccessManager::AccessStatus::
          kAvailableButNotGranted;
  bool can_request_apps_acess =
      features::IsEcheSWAEnabled() &&
      features::IsEchePhoneHubPermissionsOnboarding() &&
      multidevice_feature_access_manager->IsAccessRequestAllowed(
          Feature::kEche) &&
      multidevice_feature_access_manager->GetAppsAccessStatus() ==
          phonehub::MultideviceFeatureAccessManager::AccessStatus::
              kAvailableButNotGranted;
  bool can_request_camera_roll_access =
      features::IsPhoneHubCameraRollEnabled() &&
      multidevice_feature_access_manager->IsAccessRequestAllowed(
          Feature::kPhoneHubCameraRoll) &&
      multidevice_feature_access_manager->GetCameraRollAccessStatus() ==
          phonehub::MultideviceFeatureAccessManager::AccessStatus::
              kAvailableButNotGranted;

  PA_LOG(INFO) << "MultideviceFeatureOptInView can_request_notification_access:"
               << can_request_notification_access
               << ", can_request_apps_acess:" << can_request_apps_acess
               << ", can_request_camera_roll_access:"
               << can_request_camera_roll_access;
  if (can_request_notification_access && can_request_camera_roll_access &&
      can_request_apps_acess) {
    return PermissionSetupMode::kAllPermissionsSetupMode;
  } else if (can_request_notification_access && can_request_apps_acess &&
             !can_request_camera_roll_access) {
    return PermissionSetupMode::kNotificationAndApps;
  } else if (can_request_apps_acess && can_request_camera_roll_access &&
             !can_request_notification_access) {
    return PermissionSetupMode::kAppsAndCameraRoll;
  } else if (can_request_notification_access &&
             can_request_camera_roll_access && !can_request_apps_acess) {
    return PermissionSetupMode::kNotificationAndCameraRoll;
  } else if (!can_request_notification_access &&
             !can_request_camera_roll_access && can_request_apps_acess) {
    return PermissionSetupMode::kAppsSetupMode;
  } else if (!can_request_notification_access && !can_request_apps_acess &&
             can_request_camera_roll_access) {
    return PermissionSetupMode::kCameraRollSetupMode;
  } else if (!can_request_camera_roll_access && !can_request_apps_acess &&
             can_request_notification_access) {
    return PermissionSetupMode::kNotificationSetupMode;
  }

  return PermissionSetupMode::kNone;
}

int GetDescriptionStringId(phonehub::MultideviceFeatureAccessManager*
                               multidevice_feature_access_manager) {
  MultideviceFeatureOptInView::PermissionSetupMode permission_setup_mode =
      GetPermissionSetupMode(multidevice_feature_access_manager);
  switch (permission_setup_mode) {
    case PermissionSetupMode::kCameraRollSetupMode:
      return IDS_ASH_PHONE_HUB_CAMERA_ROLL_OPT_IN_DESCRIPTION;
    case PermissionSetupMode::kAppsSetupMode:
      return IDS_ASH_PHONE_HUB_APPS_OPT_IN_DESCRIPTION;
    case PermissionSetupMode::kNotificationAndCameraRoll:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_AND_CAMERA_ROLL_OPT_IN_DESCRIPTION;
    case PermissionSetupMode::kNotificationSetupMode:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DESCRIPTION;
    case PermissionSetupMode::kNotificationAndApps:
    case PermissionSetupMode::kAppsAndCameraRoll:
    case PermissionSetupMode::kAllPermissionsSetupMode:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_AND_APPS_OPT_IN_DESCRIPTION;
    case PermissionSetupMode::kNone:
      // Just return the default strings since the MultideviceFeatureOptInView
      // will be invisible.
      return IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DESCRIPTION;
  }
}

std::string GetMultiDeviceSettingUrl(
    PermissionSetupMode permission_setup_mode) {
  return base::StringPrintf(kMultideviceSettingsUrl,
                            static_cast<int>(permission_setup_mode));
}

}  // namespace

MultideviceFeatureOptInView::MultideviceFeatureOptInView(
    phonehub::MultideviceFeatureAccessManager*
        multidevice_feature_access_manager)
    : SubFeatureOptInView(
          PhoneHubViewID::kMultideviceFeatureOptInView,
          GetDescriptionStringId(multidevice_feature_access_manager),
          IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_SET_UP_BUTTON),
      multidevice_feature_access_manager_(multidevice_feature_access_manager) {
  DCHECK(multidevice_feature_access_manager_);
  setup_mode_ = GetPermissionSetupMode(multidevice_feature_access_manager_);
  access_manager_observation_.Observe(multidevice_feature_access_manager_);
  // Checks and updates its visibility upon creation.
  UpdateVisibility();

  // TODO: Update metric event to a non-notification specific event
  LogNotificationOptInEvent(InterstitialScreenEvent::kShown);
}

MultideviceFeatureOptInView::~MultideviceFeatureOptInView() = default;

void MultideviceFeatureOptInView::SetUpButtonPressed() {
  // Opens the notification set up dialog in settings to start the opt in flow.
  LogNotificationOptInEvent(InterstitialScreenEvent::kConfirm);
  // This intentionally uses GetInstance() to open an OS Settings page in ash.
  std::string url = GetMultiDeviceSettingUrl(setup_mode_);
  PA_LOG(INFO) << "MultideviceFeatureOptInView SetUpButtonPressed target url:"
               << url;
  NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(url), NewWindowDelegate::OpenUrlFrom::kUserInteraction);
}

void MultideviceFeatureOptInView::DismissButtonPressed() {
  // Dismiss this view if user chose to opt out and update the bubble size.
  LogNotificationOptInEvent(InterstitialScreenEvent::kDismiss);
  SetVisible(false);
  multidevice_feature_access_manager_->DismissSetupRequiredUi();
}

void MultideviceFeatureOptInView::OnNotificationAccessChanged() {
  UpdateVisibility();
}

void MultideviceFeatureOptInView::OnCameraRollAccessChanged() {
  UpdateVisibility();
}

void MultideviceFeatureOptInView::UpdateVisibility() {
  DCHECK(multidevice_feature_access_manager_);
  // Refresh the permission status.
  setup_mode_ = GetPermissionSetupMode(multidevice_feature_access_manager_);
  SetVisible(setup_mode_ != PermissionSetupMode::kNone &&
             !multidevice_feature_access_manager_
                  ->HasMultideviceFeatureSetupUiBeenDismissed());
}

BEGIN_METADATA(MultideviceFeatureOptInView, views::View)
END_METADATA

}  // namespace ash
