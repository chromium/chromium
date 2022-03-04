// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/multidevice_feature_opt_in_view.h"

#include <memory>
#include <string>

#include "ash/components/phonehub/multidevice_feature_access_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::LogInterstitialScreenEvent;

namespace {
// URL of the multidevice settings page with the URL parameter that will
// start up the opt-in-flow.
// TODO: Update this URL once the new access setup dialog has been updated
constexpr char kMultideviceSettingsUrl[] =
    "chrome://os-settings/multidevice/"
    "features?showPhonePermissionSetupDialog";

}  // namespace

MultideviceFeatureOptInView::MultideviceFeatureOptInView(
    phonehub::MultideviceFeatureAccessManager*
        multidevice_feature_access_manager)
    : SubFeatureOptInView(PhoneHubViewID::kMultideviceFeatureOptInView,
                          IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DESCRIPTION,
                          IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_SET_UP_BUTTON),
      multidevice_feature_access_manager_(multidevice_feature_access_manager) {
  DCHECK(multidevice_feature_access_manager_);
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
  NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(kMultideviceSettingsUrl),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction);
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

  // Can only request access if it is available but has not yet been granted.
  bool can_request_notification_access =
      multidevice_feature_access_manager_->GetNotificationAccessStatus() ==
      phonehub::MultideviceFeatureAccessManager::AccessStatus::
          kAvailableButNotGranted;
  bool can_request_camera_roll_access =
      features::IsPhoneHubCameraRollEnabled() &&
      multidevice_feature_access_manager_->GetCameraRollAccessStatus() ==
          phonehub::MultideviceFeatureAccessManager::AccessStatus::
              kAvailableButNotGranted;
  const bool should_show =
      (can_request_notification_access || can_request_camera_roll_access) &&
      !multidevice_feature_access_manager_
           ->HasMultideviceFeatureSetupUiBeenDismissed();
  SetVisible(should_show);
}

BEGIN_METADATA(MultideviceFeatureOptInView, views::View)
END_METADATA

}  // namespace ash
