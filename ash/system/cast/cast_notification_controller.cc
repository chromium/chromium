// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_notification_controller.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/widget/widget.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

bool ShouldShowNotification() {
  auto* cast_config = CastConfigController::Get();
  return cast_config && cast_config->HasSinksAndRoutes() &&
         cast_config->HasActiveRoute();
}

std::u16string GetNotificationTitle(const CastSink& sink,
                                    const CastRoute& route) {
  switch (route.content_source) {
    case ContentSource::kUnknown:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_CAST_UNKNOWN);
    case ContentSource::kTab:
    case ContentSource::kDesktop:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_CAST_NOTIFICATION_TITLE,
          base::UTF8ToUTF16(sink.name));
  }
}

std::u16string GetNotificationMessage(const CastRoute& route) {
  if (route.freeze_info.is_frozen) {
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_CAST_PAUSED);
  }
  switch (route.content_source) {
    case ContentSource::kUnknown:
      return std::u16string();
    case ContentSource::kTab:
      return base::UTF8ToUTF16(route.title);
    case ContentSource::kDesktop:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_CAST_CAST_DESKTOP_NOTIFICATION_MESSAGE);
  }
}

const char kNotificationId[] = "chrome://cast";
const char kNotifierId[] = "ash.cast";

}  // namespace

CastNotificationController::CastNotificationController() {
  if (CastConfigController::Get()) {
    CastConfigController::Get()->AddObserver(this);
    CastConfigController::Get()->RequestDeviceRefresh();
  }
}

CastNotificationController::~CastNotificationController() {
  if (CastConfigController::Get())
    CastConfigController::Get()->RemoveObserver(this);
}

void CastNotificationController::OnDevicesUpdated(
    const std::vector<SinkAndRoute>& devices) {
  if (!ShouldShowNotification()) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kNotificationId, false /* by_user */);
    return;
  }

  for (const auto& device : devices) {
    const CastSink& sink = device.sink;
    const CastRoute& route = device.route;

    // We only want to display casts that came from this machine, since on a
    // busy network many other people could be casting.
    if (route.id.empty() || !route.is_local_source)
      continue;

    displayed_route_id_ = route.id;

    message_center::RichNotificationData data;

    if (route.freeze_info.can_freeze) {
      displayed_route_is_frozen_ = route.freeze_info.is_frozen;
      data.buttons.emplace_back(message_center::ButtonInfo(
          displayed_route_is_frozen_
              ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_RESUME)
              : l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_PAUSE)));
      freeze_button_index_ = data.buttons.size() - 1;
    }

    data.buttons.emplace_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_STOP)));

    std::unique_ptr<Notification> notification = CreateSystemNotificationPtr(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
        GetNotificationTitle(sink, route), GetNotificationMessage(route),
        std::u16string() /* display_source */, GURL(),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
            NotificationCatalogName::kCast),
        data,
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            base::BindRepeating(&CastNotificationController::PressedCallback,
                                weak_ptr_factory_.GetWeakPtr())),
        displayed_route_is_frozen_ ? kSystemMenuCastPausedIcon
                                   : kSystemMenuCastIcon,
        message_center::SystemNotificationWarningLevel::NORMAL);
    notification->set_pinned(true);
    MessageCenter::Get()->AddNotification(std::move(notification));

    break;
  }
}

void CastNotificationController::PressedCallback(
    absl::optional<int> button_index) {
  if (freeze_button_index_ && button_index == freeze_button_index_) {
    FreezePressed();
  } else {
    // Handles the case that the stop button is pressed, or the notification is
    // pressed not on a button.
    StopCasting();
  }
}

void CastNotificationController::StopCasting() {
  CastConfigController::Get()->StopCasting(displayed_route_id_);
  base::RecordAction(base::UserMetricsAction("StatusArea_Cast_StopCast"));
}

void CastNotificationController::FreezePressed() {
  auto* controller = CastConfigController::Get();
  if (displayed_route_is_frozen_) {
    controller->UnfreezeRoute(displayed_route_id_);
  } else {
    auto* status_area_widget =
        Shell::GetPrimaryRootWindowController()->shelf()->GetStatusAreaWidget();
    if (status_area_widget->unified_system_tray() &&
        status_area_widget->unified_system_tray()
            ->IsBubbleShown()) {  // The system tray is open.
      freeze_on_tray_widget_destroyed_ = true;
      status_area_widget->unified_system_tray()->GetBubbleWidget()->AddObserver(
          this);
      status_area_widget->unified_system_tray()->CloseBubble();
    } else if (status_area_widget->notification_center_tray() &&
               status_area_widget->notification_center_tray()
                   ->IsBubbleShown()) {  // Notification tray is open.
      freeze_on_tray_widget_destroyed_ = true;
      status_area_widget->notification_center_tray()
          ->GetBubbleWidget()
          ->AddObserver(this);
      status_area_widget->notification_center_tray()->CloseBubble();
    } else {
      controller->FreezeRoute(displayed_route_id_);
    }
  }
}

void CastNotificationController::OnWidgetDestroyed(views::Widget* widget) {
  widget->RemoveObserver(this);
  if (freeze_on_tray_widget_destroyed_) {
    CastConfigController::Get()->FreezeRoute(displayed_route_id_);
    freeze_on_tray_widget_destroyed_ = false;
  }
}

}  // namespace ash
