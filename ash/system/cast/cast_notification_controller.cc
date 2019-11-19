// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

bool ShouldShowNotification() {
  auto* cast_config = CastConfigController::Get();
  return cast_config && cast_config->HasSinksAndRoutes() &&
         cast_config->HasActiveRoute();
}

base::string16 GetNotificationTitle(const CastSink& sink,
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

base::string16 GetNotificationMessage(const CastRoute& route) {
  switch (route.content_source) {
    case ContentSource::kUnknown:
      return base::string16();
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
    data.buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_STOP)));

    std::unique_ptr<Notification> notification = CreateSystemNotification(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
        GetNotificationTitle(sink, route), GetNotificationMessage(route),
        base::string16() /* display_source */, GURL(),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId),
        data,
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            base::BindRepeating(&CastNotificationController::StopCasting,
                                weak_ptr_factory_.GetWeakPtr())),
        kSystemMenuCastIcon,
        message_center::SystemNotificationWarningLevel::NORMAL);
    notification->set_pinned(true);
    MessageCenter::Get()->AddNotification(std::move(notification));

    break;
  }
}

void CastNotificationController::StopCasting() {
  CastConfigController::Get()->StopCasting(displayed_route_id_);
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_CAST_STOP_CAST);
}

}  // namespace ash
