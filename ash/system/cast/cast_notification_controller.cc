// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_notification_controller.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
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

const char kNotificationId[] = "chrome://cast";

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

  // The cast notification controller outlives cast sessions. Ensure
  // `freeze_button_index_` starts reset when creating a new notification.
  freeze_button_index_.reset();

  for (const auto& device : devices) {
    const CastSink& sink = device.sink;
    const CastRoute& route = device.route;

    // We only want to display casts that came from this machine, since on a
    // busy network many other people could be casting.
    if (route.id.empty() || !route.is_local_source)
      continue;

    displayed_route_id_ = route.id;

    message_center::RichNotificationData data;
    data.pinned = true;

    if (route.freeze_info.can_freeze) {
      displayed_route_is_frozen_ = route.freeze_info.is_frozen;

      // The new pinned notification UI uses icon instead of label buttons.
      if (features::AreOngoingProcessesEnabled()) {
        data.buttons.emplace_back(
            displayed_route_is_frozen_
                ? message_center::ButtonInfo(
                      /*vector_icon=*/&kNotificationPlayIcon,
                      /*accessible_name=*/l10n_util::GetStringUTF16(
                          IDS_ASH_STATUS_TRAY_CAST_RESUME))
                : message_center::ButtonInfo(
                      /*vector_icon=*/&kNotificationPauseIcon,
                      /*accessible_name=*/l10n_util::GetStringUTF16(
                          IDS_ASH_STATUS_TRAY_CAST_PAUSE)));
      } else {
        data.buttons.emplace_back(message_center::ButtonInfo(
            displayed_route_is_frozen_
                ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_RESUME)
                : l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_PAUSE)));
      }

      freeze_button_index_ = data.buttons.size() - 1;
    }

    // The new pinned notification UI uses icon instead of label buttons.
    if (features::AreOngoingProcessesEnabled()) {
      data.buttons.emplace_back(message_center::ButtonInfo(
          /*vector_icon=*/&kNotificationStopIcon,
          /*accessible_name=*/l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_CAST_STOP)));
    } else {
      data.buttons.emplace_back(message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_STOP)));
    }

    std::unique_ptr<Notification> notification =
        ash::SystemNotificationBuilder()
            .SetId(kNotificationId)
            .SetCatalogName(NotificationCatalogName::kCast)
            .SetTitle(GetNotificationTitle(sink, route))
            .SetOptionalFields(data)
            .SetDelegate(base::MakeRefCounted<
                         message_center::HandleNotificationClickDelegate>(
                base::BindRepeating(
                    &CastNotificationController::PressedCallback,
                    weak_ptr_factory_.GetWeakPtr())))
            .SetSmallImage(displayed_route_is_frozen_
                               ? kSystemMenuCastPausedIcon
                               : kSystemMenuCastIcon)
            .BuildPtr(
                /*keep_timestamp=*/false);

    MessageCenter::Get()->AddNotification(std::move(notification));

    break;
  }
}

void CastNotificationController::PressedCallback(
    std::optional<int> button_index) {
  if (freeze_button_index_ && button_index == freeze_button_index_) {
    FreezePressed();
  } else if (button_index) {
    // Handles the case that the stop button is pressed
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
      Shell::GetPrimaryRootWindowController()
          ->shelf()
          ->shelf_focus_cycler()
          ->FocusStatusArea(false);
      status_area_widget->unified_system_tray()->RequestFocus();
    } else if (status_area_widget->notification_center_tray() &&
               status_area_widget->notification_center_tray()
                   ->IsBubbleShown()) {  // Notification tray is open.
      freeze_on_tray_widget_destroyed_ = true;
      status_area_widget->notification_center_tray()
          ->GetBubbleWidget()
          ->AddObserver(this);
      status_area_widget->notification_center_tray()->CloseBubble();
      Shell::GetPrimaryRootWindowController()
          ->shelf()
          ->shelf_focus_cycler()
          ->FocusStatusArea(false);
      status_area_widget->notification_center_tray()->RequestFocus();
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
