// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_tray.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/notification_metrics_recorder.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_container.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"

namespace ash {

NotificationCenterTray::NotificationCenterTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kNotificationCenter,
                         RoundedCornerBehavior::kStartRounded),
      notification_grouping_controller_(
          std::make_unique<NotificationGroupingController>(this)),
      popup_collection_(std::make_unique<AshMessagePopupCollection>(
          display::Screen::GetScreen(),
          shelf)),
      notification_metrics_recorder_(
          std::make_unique<NotificationMetricsRecorder>(this)),
      notification_icons_controller_(
          std::make_unique<NotificationIconsController>(
              shelf,
              /*notification_center_tray=*/this)) {
  SetCallback(base::BindRepeating(&NotificationCenterTray::OnTrayButtonPressed,
                                  base::Unretained(this)));
  SetID(VIEW_ID_SA_NOTIFICATION_TRAY);
  set_use_bounce_in_animation(false);

  tray_container()->SetMargin(
      /*main_axis_margin=*/kUnifiedTrayContentPadding -
          ShelfConfig::Get()->status_area_hit_region_padding(),
      0);
}

NotificationCenterTray::~NotificationCenterTray() {
  for (views::View* tray_item : tray_container()->children()) {
    static_cast<TrayItemView*>(tray_item)->RemoveObserver(this);
  }
}

void NotificationCenterTray::AddNotificationCenterTrayObserver(
    Observer* observer) {
  observers_.AddObserver(observer);
}

void NotificationCenterTray::RemoveNotificationCenterTrayObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NotificationCenterTray::OnTrayItemVisibilityAboutToChange(
    bool target_visibility) {
  // A change in one of this tray's tray items could have implications for this
  // tray's overall visibility (e.g. if the only visible tray item wants to
  // become hidden, which could happen when dismissing all notifications). We
  // need to update this tray's visibility here, before the tray item gets a
  // chance to start its own visibility change animation, so that this tray does
  // not briefly become empty, for instance.
  //
  // If the tray item's visibility change does not imply a change in visibility
  // for this tray, then `SetVisiblePreferred()` (which is called by
  // `UpdateVisibility()`) will do nothing.
  UpdateVisibility();
}

void NotificationCenterTray::OnSystemTrayVisibilityChanged(
    bool system_tray_visible) {
  system_tray_visible_ = system_tray_visible;
  UpdateVisibility();
}

void NotificationCenterTray::OnTrayButtonPressed() {
  if (GetBubbleWidget()) {
    CloseBubble();
    return;
  }

  ShowBubble();
}

NotificationListView* NotificationCenterTray::GetNotificationListView() {
  if (!bubble_) {
    return nullptr;
  }

  auto* notification_center_view = bubble_->GetNotificationCenterView();
  return notification_center_view
             ? notification_center_view->notification_list_view()
             : nullptr;
}

bool NotificationCenterTray::IsBubbleShown() const {
  return !!bubble_;
}

void NotificationCenterTray::Initialize() {
  TrayBackgroundView::Initialize();

  // Add all child `TrayItemView`s.
  // TODO(b/255986529): Rewrite the `NotificationIconsController` class so that
  // we do not have to add icon views that are owned by the
  // `NotificationCenterTray` from the controller. We should make sure views are
  // only added by host views.
  notification_icons_controller_->AddNotificationTrayItems(tray_container());

  // Privacy indicator is only enabled when Video Conference is disabled.
  if (!features::IsVideoConferenceEnabled()) {
    privacy_indicators_view_ = tray_container()->AddChildView(
        std::make_unique<PrivacyIndicatorsTrayItemView>(shelf()));
  }
  for (views::View* tray_item : tray_container()->children()) {
    static_cast<TrayItemView*>(tray_item)->AddObserver(this);
  }
  for (auto& observer : observers_) {
    observer.OnAllTrayItemsAdded();
  }

  // Update this tray's visibility as well as the visibility of all of its tray
  // items according to the current state of notifications.
  UpdateVisibility();
  notification_icons_controller_->UpdateNotificationIcons();
  notification_icons_controller_->UpdateNotificationIndicators();
}

std::u16string NotificationCenterTray::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_ACCESSIBLE_NAME);
}

std::u16string NotificationCenterTray::GetAccessibleNameForTray() {
  return notification_icons_controller_->GetAccessibleNameString().value_or(
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_ACCESSIBLE_NAME));
}

void NotificationCenterTray::HandleLocaleChange() {}

void NotificationCenterTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  if (bubble_->GetBubbleView() == bubble_view) {
    CloseBubble();
  }
}

void NotificationCenterTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void NotificationCenterTray::ClickedOutsideBubble(
    const ui::LocatedEvent& event) {
  CloseBubble();
}

void NotificationCenterTray::UpdateTrayItemColor(bool is_active) {
  DCHECK(chromeos::features::IsJellyEnabled());
  for (views::View* tray_item : tray_container()->children()) {
    static_cast<TrayItemView*>(tray_item)->UpdateLabelOrImageViewColor(
        is_active);
  }
}

void NotificationCenterTray::CloseBubbleInternal() {
  if (!bubble_) {
    return;
  }

  bubble_.reset();
  SetIsActive(false);

  // Inform the message center that the bubble has closed so that popups are
  // created for new notifications.
  message_center::MessageCenter::Get()->SetVisibility(
      message_center::VISIBILITY_TRANSIENT);
}

void NotificationCenterTray::ShowBubble() {
  if (bubble_) {
    return;
  }

  // Inform the message center that the bubble is showing so that we do not
  // create popups for incoming notifications and dismiss existing popups. This
  // needs to happen before the bubble is created so that the
  // `NotificationListView` is the active `NotificationViewController` when the
  // `NotificationGroupingController` access it. This happens when notifications
  // are added to the `NotificationListView`.
  message_center::MessageCenter::Get()->SetVisibility(
      message_center::VISIBILITY_MESSAGE_CENTER);

  bubble_ = std::make_unique<NotificationCenterBubble>(this);
  bubble_->ShowBubble();

  SetIsActive(true);
}

void NotificationCenterTray::UpdateAfterLoginStatusChange() {
  UpdateVisibility();
}

TrayBubbleView* NotificationCenterTray::GetBubbleView() {
  return bubble_ ? bubble_->GetBubbleView() : nullptr;
}

views::Widget* NotificationCenterTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

void NotificationCenterTray::UpdateLayout() {
  TrayBackgroundView::UpdateLayout();

  if (privacy_indicators_view_) {
    privacy_indicators_view_->UpdateAlignmentForShelf(shelf());
  }
}

void NotificationCenterTray::UpdateVisibility() {
  // `NotificationIconsController` handles updating this tray's tray items, so
  // no need to do that here.
  const bool new_visibility =
      message_center::MessageCenter::Get()->NotificationCount() > 0 &&
      system_tray_visible_;
  SetVisiblePreferred(new_visibility);
  if (chromeos::features::IsJellyEnabled()) {
    UpdateTrayItemColor(is_active());
  }

  // We should close the bubble if there are no more notifications to show.
  if (!new_visibility && bubble_) {
    CloseBubble();
  }
}

BEGIN_METADATA(NotificationCenterTray)
END_METADATA

}  // namespace ash
