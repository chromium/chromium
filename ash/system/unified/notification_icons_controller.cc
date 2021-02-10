// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_icons_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/message_center/message_center_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// Maximum number of notification icons shown in the system tray button.
constexpr int kMaxNotificationIconsShown = 2;

// We only show notification icon in the tray if it is either:
// *   Pinned (generally used for background process such as sharing your
//     screen, capslock, etc.).
// *   Critical warning (display failure, disk space critically low, etc.).
bool ShouldShowNotification(message_center::Notification* notification) {
  return notification->pinned() ||
         notification->system_notification_warning_level() ==
             message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
}

}  // namespace

NotificationIconTrayItemView::NotificationIconTrayItemView(Shelf* shelf)
    : TrayItemView(shelf) {
  CreateImageView();
}

NotificationIconTrayItemView::~NotificationIconTrayItemView() = default;

void NotificationIconTrayItemView::SetNotification(
    message_center::Notification* notification) {
  notification_ = notification;
  notification_id_ = notification->id();

  gfx::Image masked_small_icon = notification_->GenerateMaskedSmallIcon(
      kUnifiedTrayIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));
  if (!masked_small_icon.IsEmpty()) {
    image_view()->SetImage(masked_small_icon.AsImageSkia());
  } else {
    image_view()->SetImage(gfx::CreateVectorIcon(
        message_center::kProductIcon, kUnifiedTrayIconSize,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
  }

  UpdateTooltipText();
}

void NotificationIconTrayItemView::Reset() {
  notification_ = nullptr;
  notification_id_ = std::string();
  image_view()->SetImage(gfx::ImageSkia());
  image_view()->SetTooltipText(base::string16());
}

void NotificationIconTrayItemView::UpdateTooltipText() {
  image_view()->SetTooltipText(notification_->title());
}

bool NotificationIconTrayItemView::HasNotification() {
  return notification_;
}

const std::string& NotificationIconTrayItemView::GetNotificationId() const {
  return notification_id_;
}

void NotificationIconTrayItemView::HandleLocaleChange() {
  UpdateTooltipText();
}

const char* NotificationIconTrayItemView::GetClassName() const {
  return "NotificationIconTrayItemView";
}

NotificationIconsController::NotificationIconsController(
    UnifiedSystemTray* tray)
    : tray_(tray) {
  system_tray_model_observation_.Observe(tray_->model());
  message_center::MessageCenter::Get()->AddObserver(this);
}

NotificationIconsController::~NotificationIconsController() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
}

void NotificationIconsController::AddNotificationTrayItems(
    TrayContainer* tray_container) {
  for (int i = 0; i < kMaxNotificationIconsShown; ++i) {
    tray_items_.push_back(tray_container->AddChildView(
        std::make_unique<NotificationIconTrayItemView>(tray_->shelf())));
  }

  OnSystemTrayButtonSizeChanged(tray_->model()->GetSystemTrayButtonSize());
}

void NotificationIconsController::OnSystemTrayButtonSizeChanged(
    UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) {
  icons_view_visible_ =
      system_tray_size != UnifiedSystemTrayModel::SystemTrayButtonSize::kSmall;
  Update();
}

void NotificationIconsController::OnNotificationAdded(const std::string& id) {
  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!ShouldShowNotification(notification))
    return;

  // Reset the notification icons if a notification is added since we don't
  // know the position where its icon should be added.
  Update();
}

void NotificationIconsController::OnNotificationRemoved(const std::string& id,
                                                        bool by_user) {
  // If the notification removed is displayed in an icon, call update to show
  // another notification if needed.
  if (GetNotificationIconShownInTray(id))
    Update();
}

void NotificationIconsController::OnNotificationUpdated(const std::string& id) {
  NotificationIconTrayItemView* item = GetNotificationIconShownInTray(id);
  if (item)
    item->UpdateTooltipText();
}

void NotificationIconsController::Update() {
  auto it = tray_items_.begin();
  for (message_center::Notification* notification :
       message_center_utils::GetSortedVisibleNotifications()) {
    if (it == tray_items_.end())
      break;
    if (ShouldShowNotification(notification)) {
      (*it)->SetNotification(notification);
      (*it)->SetVisible(icons_view_visible_);
      ++it;
    }
  }

  first_unused_item_index_ = std::distance(tray_items_.begin(), it);

  for (; it != tray_items_.end(); ++it) {
    (*it)->Reset();
    (*it)->SetVisible(false);
  }
}

NotificationIconTrayItemView*
NotificationIconsController::GetNotificationIconShownInTray(
    const std::string& id) {
  for (NotificationIconTrayItemView* tray_item : tray_items_) {
    if (tray_item->GetNotificationId() == id)
      return tray_item;
  }
  return nullptr;
}

}  // namespace ash
