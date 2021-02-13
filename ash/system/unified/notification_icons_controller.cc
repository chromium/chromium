// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_icons_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/message_center_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {

// Maximum number of notification icons shown in the system tray button.
constexpr int kMaxNotificationIconsShown = 2;
constexpr int kSeparatorPadding = 3;

// We only show notification icon in the tray if it is either:
// *   Pinned (generally used for background process such as sharing your
//     screen, capslock, etc.).
// *   Critical warning (display failure, disk space critically low, etc.).
bool ShouldShowNotification(message_center::Notification* notification) {
  return notification->pinned() ||
         notification->system_notification_warning_level() ==
             message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
}

class SeparatorTrayItemView : public TrayItemView {
 public:
  explicit SeparatorTrayItemView(Shelf* shelf) : TrayItemView(shelf) {
    views::Separator* separator = new views::Separator();
    separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kSeparatorColor));
    separator->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kSeparatorPadding)));
    AddChildView(separator);
  }
  ~SeparatorTrayItemView() override = default;
  SeparatorTrayItemView(const SeparatorTrayItemView&) = delete;
  SeparatorTrayItemView& operator=(const SeparatorTrayItemView&) = delete;

  // TrayItemView:
  void HandleLocaleChange() override {}
  const char* GetClassName() const override { return "SeparatorTrayItemView"; }
};

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

  hidden_notification_count_view_ = tray_container->AddChildView(
      std::make_unique<HiddenNotificationCountView>(tray_->shelf()));

  separator_ = tray_container->AddChildView(
      std::make_unique<SeparatorTrayItemView>(tray_->shelf()));

  OnSystemTrayButtonSizeChanged(tray_->model()->GetSystemTrayButtonSize());
}

void NotificationIconsController::UpdateHiddenNotificationCounter() {
  if (!icons_view_visible_ || !TrayItemHasNotification()) {
    hidden_notification_count_view_->SetVisible(false);
    return;
  }

  // `first_unused_item_index_` is also the total number of notification icons
  // shown in the tray.
  int hidden_notification_num =
      message_center_utils::GetNotificationCount() - first_unused_item_index_;
  if (hidden_notification_num != 0)
    hidden_notification_count_view_->label()->SetText(
        l10n_util::GetStringFUTF16Int(
            IDS_ASH_STATUS_TRAY_HIDDEN_NOTIFICATION_COUNT_LABEL,
            hidden_notification_num));

  hidden_notification_count_view_->SetVisible(hidden_notification_num != 0);
}

bool NotificationIconsController::TrayItemHasNotification() {
  return first_unused_item_index_ != 0;
}

void NotificationIconsController::OnSystemTrayButtonSizeChanged(
    UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) {
  icons_view_visible_ =
      system_tray_size != UnifiedSystemTrayModel::SystemTrayButtonSize::kSmall;
  UpdateNotificationIcons();
  UpdateHiddenNotificationCounter();
}

void NotificationIconsController::OnNotificationAdded(const std::string& id) {
  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!ShouldShowNotification(notification))
    return;

  // Reset the notification icons if a notification is added since we don't
  // know the position where its icon should be added.
  UpdateNotificationIcons();
}

void NotificationIconsController::OnNotificationRemoved(const std::string& id,
                                                        bool by_user) {
  // If the notification removed is displayed in an icon, call update to show
  // another notification if needed.
  if (GetNotificationIconShownInTray(id))
    UpdateNotificationIcons();
}

void NotificationIconsController::OnNotificationUpdated(const std::string& id) {
  NotificationIconTrayItemView* item = GetNotificationIconShownInTray(id);
  if (item)
    item->UpdateTooltipText();
}

void NotificationIconsController::UpdateNotificationIcons() {
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
  separator_->SetVisible(icons_view_visible_ && TrayItemHasNotification());
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
