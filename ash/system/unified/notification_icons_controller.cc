// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_icons_controller.h"

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_utils.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_animation_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {

constexpr int kIconsViewDisplaySizeThreshold = 768;

// Maximum number of notification icons shown in the system tray button.
constexpr int kMaxNotificationIconsShown = 2;

const char kCapsLockNotifierId[] = "ash.caps-lock";
const char kBatteryNotificationNotifierId[] = "ash.battery";
const char kUsbNotificationNotifierId[] = "ash.power";

bool ShouldShowNotification(message_center::Notification* notification) {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  if (!session_controller->ShouldShowNotificationTray() ||
      (session_controller->IsScreenLocked() &&
       !AshMessageCenterLockScreenController::IsEnabled())) {
    return false;
  }

  std::string notifier_id = notification->notifier_id().id;

  if (message_center::MessageCenter::Get()->IsQuietMode() &&
      notifier_id != kCapsLockNotifierId) {
    return false;
  }

  // We don't want to show these notifications since the information is
  // already indicated by another item in tray.
  if (notifier_id == kVmCameraMicNotifierId ||
      notifier_id == kBatteryNotificationNotifierId ||
      notifier_id == kUsbNotificationNotifierId ||
      notifier_id == kPrivacyIndicatorsNotifierId) {
    return false;
  }

  // We only show notification icon in the tray if it is either:
  // *   Pinned (generally used for background process such as sharing your
  //     screen, capslock, etc.).
  // *   Critical warning (display failure, disk space critically low, etc.).
  return notification->pinned() ||
         notification->system_notification_warning_level() ==
             message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
}

}  // namespace

NotificationIconTrayItemView::NotificationIconTrayItemView(
    Shelf* shelf,
    NotificationIconsController* controller)
    : TrayItemView(shelf), controller_(controller) {
  CreateImageView();
}

NotificationIconTrayItemView::~NotificationIconTrayItemView() = default;

void NotificationIconTrayItemView::SetNotification(
    message_center::Notification* notification) {
  notification_id_ = notification->id();
  notification_ = notification->DeepCopy(
      *notification, GetColorProvider(), /*include_body_image=*/true,
      /*include_small_image=*/true, /*include_icon_images=*/true);

  UpdateImageViewColor();
  image_view()->SetTooltipText(notification->title());
}

void NotificationIconTrayItemView::MaybeReset() {
  if (!features::IsQsRevampEnabled()) {
    Reset();
    return;
  }
  if ((!target_visible() && IsAnimating()) ||
      shelf()
          ->status_area_widget()
          ->animation_controller()
          ->is_hide_animation_scheduled()) {
    return;
  }
  Reset();
}

void NotificationIconTrayItemView::Reset() {
  notification_id_ = std::string();
  notification_.reset();
  image_view()->SetImage(gfx::ImageSkia());
  image_view()->SetTooltipText(std::u16string());
}

void NotificationIconTrayItemView::ImmediatelyUpdateVisibility() {
  TrayItemView::ImmediatelyUpdateVisibility();
  if (features::IsQsRevampEnabled() && !target_visible()) {
    Reset();
  }
}

void NotificationIconTrayItemView::AnimationEnded(
    const gfx::Animation* animation) {
  TrayItemView::AnimationEnded(animation);
  if (features::IsQsRevampEnabled() && !target_visible()) {
    Reset();
  }
}

const std::u16string& NotificationIconTrayItemView::GetAccessibleNameString()
    const {
  if (notification_id_.empty())
    return base::EmptyString16();
  return image_view()->GetTooltipText();
}

const std::string& NotificationIconTrayItemView::GetNotificationId() const {
  return notification_id_;
}

void NotificationIconTrayItemView::HandleLocaleChange() {}

const char* NotificationIconTrayItemView::GetClassName() const {
  return "NotificationIconTrayItemView";
}

void NotificationIconTrayItemView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  controller_->UpdateNotificationIcons();
}

void NotificationIconTrayItemView::UpdateLabelOrImageViewColor(bool active) {
  if (!chromeos::features::IsJellyEnabled()) {
    return;
  }
  TrayItemView::UpdateLabelOrImageViewColor(active);

  UpdateImageViewColor();
}

void NotificationIconTrayItemView::UpdateImageViewColor() {
  if (!GetWidget() || !notification_) {
    return;
  }

  const auto* color_provider = GetColorProvider();
  ui::ColorId color_id = kColorAshIconColorPrimary;
  if (chromeos::features::IsJellyEnabled()) {
    color_id = is_active() ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                           : cros_tokens::kCrosSysOnSurface;
  }
  gfx::Image masked_small_icon = notification_->GenerateMaskedSmallIcon(
      kUnifiedTrayIconSize, color_provider->GetColor(color_id),
      color_provider->GetColor(ui::kColorNotificationIconBackground),
      color_provider->GetColor(ui::kColorNotificationIconForeground));
  if (!masked_small_icon.IsEmpty()) {
    image_view()->SetImage(masked_small_icon.AsImageSkia());
  } else {
    image_view()->SetImage(ui::ImageModel::FromVectorIcon(
        message_center::kProductIcon, color_id, kUnifiedTrayIconSize));
  }
}

NotificationIconsController::NotificationIconsController(
    Shelf* shelf,
    UnifiedSystemTrayModel* system_tray_model,
    NotificationCenterTray* notification_center_tray)
    : shelf_(shelf),
      system_tray_model_(system_tray_model),
      notification_center_tray_(notification_center_tray) {
  // When the QS revamp is enabled `notification_center_tray` should not be
  // null.
  DCHECK(!features::IsQsRevampEnabled() || notification_center_tray);
  if (system_tray_model) {
    // `UnifiedSystemTrayModel` should not be used once the kQsRevamp feature is
    // enabled. Once kQsRevamp is enabled `UnifiedSystemTrayModel` and
    // `NotificationIconsController will have different owner classes so we need
    // to remove any dependencies for this class in `UnifiedSystemTrayModel`.
    DCHECK(!features::IsQsRevampEnabled());
    system_tray_model_observation_.Observe(system_tray_model);
  }

  // Initialize `icons_view_visible_` according to display size. Only do this
  // when QsRevamp is enabled; without QsRevamp, icons view visibility is
  // determined by the `UnifiedSystemTrayModel`.
  if (features::IsQsRevampEnabled()) {
    UpdateIconsViewVisibleForDisplaySize();
  }

  message_center::MessageCenter::Get()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
}

NotificationIconsController::~NotificationIconsController() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void NotificationIconsController::AddNotificationTrayItems(
    TrayContainer* tray_container) {
  for (int i = 0; i < kMaxNotificationIconsShown; ++i) {
    tray_items_.push_back(tray_container->AddChildView(
        std::make_unique<NotificationIconTrayItemView>(shelf_,
                                                       /*controller=*/this)));
  }

  notification_counter_view_ = tray_container->AddChildView(
      std::make_unique<NotificationCounterView>(shelf_, /*controller=*/this));

  quiet_mode_view_ =
      tray_container->AddChildView(std::make_unique<QuietModeView>(shelf_));

  // `separator_` is only shown in the `UnifiedSystemTray` with the QsRevamp
  // feature disabled. The `separator_` will not be needed once kQsRevamp
  // launches because the icons related to this controller will have their own
  // dedicated tray button.
  if (!features::IsQsRevampEnabled()) {
    separator_ = tray_container->AddChildView(
        std::make_unique<SeparatorTrayItemView>(shelf_));
  }

  if (!features::IsQsRevampEnabled()) {
    OnSystemTrayButtonSizeChanged(
        system_tray_model_->GetSystemTrayButtonSize());
  }
}

bool NotificationIconsController::TrayItemHasNotification() const {
  return first_unused_item_index_ != 0;
}

size_t NotificationIconsController::TrayNotificationIconsCount() const {
  // `first_unused_item_index_` is also the total number of notification icons
  // shown in the tray.
  return first_unused_item_index_;
}

std::u16string NotificationIconsController::GetAccessibleNameString() const {
  if (!TrayItemHasNotification())
    return notification_counter_view_->GetAccessibleNameString();

  std::vector<std::u16string> status;
  status.push_back(l10n_util::GetPluralStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_IMPORTANT_COUNT_ACCESSIBLE_NAME,
      TrayNotificationIconsCount()));
  for (NotificationIconTrayItemView* tray_item : tray_items_) {
    status.push_back(tray_item->GetAccessibleNameString());
  }
  status.push_back(notification_counter_view_->GetAccessibleNameString());
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_ICONS_ACCESSIBLE_NAME, status, nullptr);
}

void NotificationIconsController::UpdateNotificationIcons() {
  // Iterates `tray_items_` and notifications in reverse order so new pinned
  // notifications get shown on the left side.
  auto notifications =
      message_center_utils::GetSortedNotificationsWithOwnView();

  auto tray_it = tray_items_.rbegin();
  for (auto notification_it = notifications.rbegin();
       notification_it != notifications.rend(); ++notification_it) {
    if (tray_it == tray_items_.rend()) {
      break;
    }

    if (ShouldShowNotification(*notification_it)) {
      (*tray_it)->SetNotification(*notification_it);
      (*tray_it)->SetVisible(icons_view_visible_);
      ++tray_it;
    }
  }

  first_unused_item_index_ = std::distance(tray_items_.rbegin(), tray_it);

  for (; tray_it != tray_items_.rend(); ++tray_it) {
    // Note: It is important to set the visibility before resetting so that the
    // icon image does not disappear while the tray item is still visible.
    (*tray_it)->SetVisible(false);
    (*tray_it)->MaybeReset();
  }

  if (separator_) {
    separator_->SetVisible(icons_view_visible_ && TrayItemHasNotification());
  }
}

void NotificationIconsController::UpdateNotificationIndicators() {
  notification_counter_view_->Update();
  quiet_mode_view_->Update();
}

void NotificationIconsController::UpdateIconsViewVisibleForDisplaySize() {
  aura::Window* window = shelf_->status_area_widget()->GetNativeWindow();
  auto display = display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const int display_size =
      std::max(display.size().width(), display.size().height());
  icons_view_visible_ = display_size >= kIconsViewDisplaySizeThreshold;
}

void NotificationIconsController::OnSystemTrayButtonSizeChanged(
    UnifiedSystemTrayModel::SystemTrayButtonSize unified_system_tray_size) {
  icons_view_visible_ = unified_system_tray_size !=
                        UnifiedSystemTrayModel::SystemTrayButtonSize::kSmall;
  UpdateNotificationIcons();
  UpdateNotificationIndicators();
}

void NotificationIconsController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // Icons view visibility is determined by the `UnifiedSystemTrayModel`
  // without the QsRevamp feature.
  // TODO(b/256692263): Remove `OnSystemTrayButtonSizeChanged` from
  // `UnifiedSystemTrayModel` once QsRevamp launches. It is only used to update
  // icon visibility for notifications in the tray.
  if (!features::IsQsRevampEnabled())
    return;

  aura::Window* window = shelf_->status_area_widget()->GetNativeWindow();
  if (display::Screen::GetScreen()->GetDisplayNearestWindow(window).id() !=
      display.id()) {
    return;
  }
  auto old_icons_view_visible = icons_view_visible_;
  UpdateIconsViewVisibleForDisplaySize();
  if (old_icons_view_visible == icons_view_visible_)
    return;

  UpdateNotificationIcons();
  UpdateNotificationIndicators();
}

void NotificationIconsController::OnNotificationAdded(const std::string& id) {
  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(id);
  // `notification` is null if it is not visible.
  if (notification && ShouldShowNotification(notification)) {
    // Reset the notification icons if a notification is added since we don't
    // know the position where its icon should be added.
    UpdateNotificationIcons();
  }

  UpdateNotificationIndicators();

  if (features::IsQsRevampEnabled()) {
    // There are certain sequences of notification updates that can require
    // explicitly updating the notification center tray even after its tray
    // items have been updated - for instance, adding an initial notification on
    // the lock screen. See http://b/297579552.
    notification_center_tray_->UpdateVisibility();
  }
}

void NotificationIconsController::OnNotificationRemoved(const std::string& id,
                                                        bool by_user) {
  // If the notification removed is displayed in an icon, call update to show
  // another notification if needed.
  if (GetNotificationIconShownInTray(id))
    UpdateNotificationIcons();

  UpdateNotificationIndicators();

  if (features::IsQsRevampEnabled()) {
    // There are certain sequences of notification updates that can require
    // explicitly updating the notification center tray even after its tray
    // items have been updated - for instance, removing a notification group's
    // parent notification when the only remaining notifications belong to that
    // group. See http://b/296918234.
    notification_center_tray_->UpdateVisibility();
  }
}

void NotificationIconsController::OnNotificationUpdated(const std::string& id) {
  // A notification update may impact certain notification icon(s) visibility
  // in the tray, so update all notification icons.
  UpdateNotificationIcons();
  UpdateNotificationIndicators();
}

void NotificationIconsController::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  if (features::IsQsRevampEnabled()) {
    UpdateNotificationIcons();
    UpdateNotificationIndicators();
  }
}

void NotificationIconsController::OnQuietModeChanged(bool in_quiet_mode) {
  UpdateNotificationIcons();
  UpdateNotificationIndicators();
}

void NotificationIconsController::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateNotificationIcons();
  UpdateNotificationIndicators();

  if (separator_)
    separator_->UpdateColor(state);
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
