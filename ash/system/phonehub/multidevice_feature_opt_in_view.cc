// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/multidevice_feature_opt_in_view.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/status_area_widget.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

using ash::phonehub::util::LogPermissionOnboardingPromoAction;
using ash::phonehub::util::LogPermissionOnboardingPromoShown;
using ash::phonehub::util::PermissionsOnboardingScreenEvent;
using ash::phonehub::util::PermissionsOnboardingSetUpMode;
using multidevice_setup::mojom::Feature;

namespace {

// URL of the multidevice settings page with the URL parameter that will
// start up the opt-in-flow.
// TODO: Update this URL once the new access setup dialog has been updated
constexpr char kMultideviceSettingsUrl[] =
    "chrome://os-settings/multidevice/"
    "features?showPhonePermissionSetupDialog&mode=%d";

PermissionsOnboardingSetUpMode GetPermissionSetupMode(
    phonehub::MultideviceFeatureAccessManager*
        multidevice_feature_access_manager) {
  bool can_request_notification_access =
      multidevice_feature_access_manager->GetNotificationAccessStatus() ==
      phonehub::MultideviceFeatureAccessManager::AccessStatus::
          kAvailableButNotGranted;
  bool can_request_apps_acess =
      features::IsEcheSWAEnabled() &&
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
    return PermissionsOnboardingSetUpMode::kAllPermissions;
  } else if (can_request_notification_access && can_request_apps_acess &&
             !can_request_camera_roll_access) {
    return PermissionsOnboardingSetUpMode::kNotificationAndMessagingApps;
  } else if (can_request_apps_acess && can_request_camera_roll_access &&
             !can_request_notification_access) {
    return PermissionsOnboardingSetUpMode::kMessagingAppsAndCameraRoll;
  } else if (can_request_notification_access &&
             can_request_camera_roll_access && !can_request_apps_acess) {
    return PermissionsOnboardingSetUpMode::kNotificationAndCameraRoll;
  } else if (!can_request_notification_access &&
             !can_request_camera_roll_access && can_request_apps_acess) {
    return PermissionsOnboardingSetUpMode::kMessagingApps;
  } else if (!can_request_notification_access && !can_request_apps_acess &&
             can_request_camera_roll_access) {
    return PermissionsOnboardingSetUpMode::kCameraRoll;
  } else if (!can_request_camera_roll_access && !can_request_apps_acess &&
             can_request_notification_access) {
    return PermissionsOnboardingSetUpMode::kNotification;
  }

  return PermissionsOnboardingSetUpMode::kNone;
}

std::string GetMultiDeviceSettingUrl(
    PermissionsOnboardingSetUpMode permission_setup_mode) {
  return base::StringPrintf(kMultideviceSettingsUrl,
                            static_cast<int>(permission_setup_mode));
}

}  // namespace

MultideviceFeatureOptInView::MultideviceFeatureOptInView(
    phonehub::MultideviceFeatureAccessManager*
        multidevice_feature_access_manager)
    : SubFeatureOptInView(
          PhoneHubViewID::kMultideviceFeatureOptInView,
          GetPermissionSetupMode(multidevice_feature_access_manager)),
      multidevice_feature_access_manager_(multidevice_feature_access_manager) {
  DCHECK(multidevice_feature_access_manager_);
  setup_mode_ = GetPermissionSetupMode(multidevice_feature_access_manager_);
  access_manager_observation_.Observe(
      multidevice_feature_access_manager_.get());
  // Checks and updates its visibility upon creation.
  UpdateVisibility(/*was_visible=*/false);
}

MultideviceFeatureOptInView::~MultideviceFeatureOptInView() = default;

void MultideviceFeatureOptInView::SetUpButtonPressed() {
  // Opens the set up dialog in settings to start the opt in flow.
  LogPermissionOnboardingPromoAction(
      PermissionsOnboardingScreenEvent::kSetUpOrDone);
  // This intentionally uses GetInstance() to open an OS Settings page in ash.
  std::string url = GetMultiDeviceSettingUrl(setup_mode_);
  PA_LOG(INFO) << "MultideviceFeatureOptInView SetUpButtonPressed target url:"
               << url;
  NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(url), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  ClosePhoneHubBubble();
}

void MultideviceFeatureOptInView::DismissButtonPressed() {
  // Dismiss this view if user chose to opt out and update the bubble size.
  LogPermissionOnboardingPromoAction(
      PermissionsOnboardingScreenEvent::kDismissOrCancel);
  SetVisible(false);
  multidevice_feature_access_manager_->DismissSetupRequiredUi();
}

void MultideviceFeatureOptInView::OnNotificationAccessChanged() {
  UpdateVisibility(/*was_visible=*/GetVisible());
}

void MultideviceFeatureOptInView::OnCameraRollAccessChanged() {
  UpdateVisibility(/*was_visible=*/GetVisible());
}

void MultideviceFeatureOptInView::UpdateVisibility(bool was_visible) {
  DCHECK(multidevice_feature_access_manager_);
  // Refresh the permission status if changed
  phonehub::util::PermissionsOnboardingSetUpMode current_mode =
      GetPermissionSetupMode(multidevice_feature_access_manager_);
  if (current_mode != setup_mode_) {
    setup_mode_ = current_mode;
    SetSetUpMode(setup_mode_);
  }
  SetVisible(setup_mode_ != PermissionsOnboardingSetUpMode::kNone &&
             !multidevice_feature_access_manager_
                  ->HasMultideviceFeatureSetupUiBeenDismissed());
  if (!was_visible && GetVisible()) {
    LogPermissionOnboardingPromoShown(setup_mode_);
  }
  PreferredSizeChanged();
}

void MultideviceFeatureOptInView::ClosePhoneHubBubble() {
  // Close Phone Hub bubble in current display.
  views::Widget* const widget = GetWidget();
  // |widget| is null when this function is called before the view is added to a
  // widget (in unit tests).
  if (!widget) {
    return;
  }
  int64_t current_display_id =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(widget->GetNativeWindow())
          .id();
  Shell::GetRootWindowControllerWithDisplayId(current_display_id)
      ->GetStatusAreaWidget()
      ->phone_hub_tray()
      ->CloseBubble();
}

BEGIN_METADATA(MultideviceFeatureOptInView)
END_METADATA

}  // namespace ash
