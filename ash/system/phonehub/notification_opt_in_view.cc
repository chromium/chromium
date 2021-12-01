// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/notification_opt_in_view.h"

#include <memory>
#include <string>

#include "ash/components/phonehub/notification_access_manager.h"
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
constexpr char kMultideviceSettingsUrl[] =
    "chrome://os-settings/multidevice/"
    "features?showNotificationAccessSetupDialog";

}  // namespace

NotificationOptInView::NotificationOptInView(
    phonehub::NotificationAccessManager* notification_access_manager)
    : SubFeatureOptInView(PhoneHubViewID::kNotificationOptInView,
                          IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DESCRIPTION,
                          IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_SET_UP_BUTTON),
      notification_access_manager_(notification_access_manager) {
  DCHECK(notification_access_manager_);
  access_manager_observation_.Observe(notification_access_manager_);

  // Checks and updates its visibility upon creation.
  UpdateVisibility();

  LogNotificationOptInEvent(InterstitialScreenEvent::kShown);
}

NotificationOptInView::~NotificationOptInView() = default;

void NotificationOptInView::SetUpButtonPressed() {
  // Opens the notification set up dialog in settings to start the opt in flow.
  LogNotificationOptInEvent(InterstitialScreenEvent::kConfirm);
  NewWindowDelegate::GetPrimary()->OpenUrl(GURL(kMultideviceSettingsUrl),
                                           /*from_user_interaction=*/true);
}

void NotificationOptInView::DismissButtonPressed() {
  // Dismiss this view if user chose to opt out and update the bubble size.
  LogNotificationOptInEvent(InterstitialScreenEvent::kDismiss);
  SetVisible(false);
  notification_access_manager_->DismissSetupRequiredUi();
}

void NotificationOptInView::OnNotificationAccessChanged() {
  UpdateVisibility();
}

void NotificationOptInView::UpdateVisibility() {
  DCHECK(notification_access_manager_);

  // Can only request access if it is available but has not yet been granted.
  bool can_request_access = notification_access_manager_->GetAccessStatus() ==
                            phonehub::NotificationAccessManager::AccessStatus::
                                kAvailableButNotGranted;
  const bool should_show =
      can_request_access &&
      !notification_access_manager_->HasNotificationSetupUiBeenDismissed();
  SetVisible(should_show);
}

BEGIN_METADATA(NotificationOptInView, views::View)
END_METADATA

}  // namespace ash
