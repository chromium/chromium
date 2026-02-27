// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_notification_observer.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_changed_notification_handler.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace default_browser {

DefaultBrowserNotificationObserver::DefaultBrowserNotificationObserver(
    RegisterCallback register_callback,
    InitialStateCheckCallback initial_state_check_callback,
    DefaultBrowserManager& manager)
    : manager_(manager) {
  default_browser_change_subscription_ =
      std::move(register_callback)
          .Run(base::BindRepeating(
              &DefaultBrowserNotificationObserver::OnDefaultBrowserStateChanged,
              base::Unretained(this)));

  std::move(initial_state_check_callback)
      .Run(base::BindOnce(
          &DefaultBrowserNotificationObserver::OnDefaultBrowserStateChanged,
          base::Unretained(this)));
}

DefaultBrowserNotificationObserver::~DefaultBrowserNotificationObserver() =
    default;

void DefaultBrowserNotificationObserver::OnDefaultBrowserStateChanged(
    DefaultBrowserState state) {
  bool lost_default_status = (last_state_ == shell_integration::IS_DEFAULT &&
                              state == shell_integration::NOT_DEFAULT);

  last_state_ = state;

  if (!lost_default_status) {
    return;
  }

  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_YES_BUTTON));
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_NO_THANKS_BUTTON));
  optional_fields.accessible_name =
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_MESSAGE);

  const gfx::Image product_logo =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_PRODUCT_LOGO_128);

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      DefaultBrowserManager::kNotificationId,
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_TITLE),
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_MESSAGE),
      ui::ImageModel::FromImage(product_logo),
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 DefaultBrowserManager::kNotificationId),
      optional_fields, /*delegate=*/nullptr);

  NotificationDisplayServiceFactory::GetForProfile(&manager_->GetProfile())
      ->Display(NotificationHandler::Type::DEFAULT_BROWSER_CHANGED,
                std::move(notification), /*metadata=*/nullptr);
}

}  // namespace default_browser
