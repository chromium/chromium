// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/system_proxy_notification.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace {
constexpr char kNotificationId[] = "system-proxy.auth_required";
constexpr char kNotifierId[] = "system-proxy";
}  // namespace

namespace ash {

SystemProxyNotification::SystemProxyNotification(
    const system_proxy::ProtectionSpace& protection_space,
    bool show_error,
    OnClickCallback callback)
    : protection_space_(protection_space),
      show_error_(show_error),
      on_click_callback_(std::move(callback)),
      weak_ptr_factory_(this) {}

SystemProxyNotification::~SystemProxyNotification() = default;

void SystemProxyNotification::Show() {
  const std::u16string title =
      show_error_ ? l10n_util::GetStringUTF16(
                        IDS_SYSTEM_PROXY_AUTH_REQUIRED_NOTIFICATION_ERROR_TITLE)
                  : l10n_util::GetStringUTF16(
                        IDS_SYSTEM_PROXY_AUTH_REQUIRED_NOTIFICATION_TITLE);
  const std::u16string body = l10n_util::GetStringFUTF16(
      IDS_SYSTEM_PROXY_AUTH_REQUIRED_NOTIFICATION_BODY,
      base::ASCIIToUTF16(protection_space_.origin()));

  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title, body,
      std::u16string() /*display_source=*/, GURL() /*origin_url=*/,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kSystemProxy),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&SystemProxyNotification::OnClick,
                              weak_ptr_factory_.GetWeakPtr())),
      kNotificationWifiIcon,
      message_center::SystemNotificationWarningLevel::WARNING);

  notification.set_pinned(true);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

void SystemProxyNotification::SystemProxyNotification::OnClick() {
  DCHECK(!on_click_callback_.is_null());
  std::move(on_click_callback_).Run(protection_space_, show_error_);
}

void SystemProxyNotification::Close() {
  SystemNotificationHelper::GetInstance()->Close(kNotificationId);
}

}  // namespace ash
