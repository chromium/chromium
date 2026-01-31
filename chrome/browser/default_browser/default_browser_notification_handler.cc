// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_notification_handler.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
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

namespace {

// Delegate to route notification events back to the handler.
class DefaultBrowserNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit DefaultBrowserNotificationDelegate(
      base::WeakPtr<DefaultBrowserNotificationHandler> handler)
      : handler_(handler) {}

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    if (handler_) {
      handler_->OnNotificationClick(button_index);
    }
  }

  void Close(bool by_user) override {
    if (handler_) {
      handler_->OnNotificationClose(by_user);
    }
  }

 private:
  ~DefaultBrowserNotificationDelegate() override = default;
  base::WeakPtr<DefaultBrowserNotificationHandler> handler_;
};

}  // namespace

DefaultBrowserNotificationHandler::DefaultBrowserNotificationHandler(
    DefaultBrowserManager& manager)
    : manager_(manager) {
  default_browser_change_subscription_ =
      manager.RegisterDefaultBrowserChanged(base::BindRepeating(
          &DefaultBrowserNotificationHandler::OnDefaultBrowserStateChanged,
          base::Unretained(this)));
}

DefaultBrowserNotificationHandler::~DefaultBrowserNotificationHandler() =
    default;

void DefaultBrowserNotificationHandler::OnDefaultBrowserStateChanged(
    DefaultBrowserState state) {
  if (state != shell_integration::NOT_DEFAULT) {
    return;
  }

  auto delegate = base::MakeRefCounted<DefaultBrowserNotificationDelegate>(
      weak_ptr_factory_.GetWeakPtr());

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
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_TITLE),
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_MESSAGE),
      ui::ImageModel::FromImage(product_logo),
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotificationId),
      optional_fields, delegate);

  SystemNotificationHelper::GetInstance()->Display(std::move(notification));

  // Create a new controller for this interaction.
  controller_ = manager_->CreateControllerFor(
      DefaultBrowserEntrypointType::kChangeDetectedNotification);
  CHECK(controller_);
  controller_->OnShown();
}

void DefaultBrowserNotificationHandler::OnNotificationClick(
    std::optional<int> button_index) {
  auto controller = std::exchange(controller_, nullptr);
  if (!controller) {
    return;
  }

  if (!button_index.has_value() || button_index.value() == 0) {
    auto* controller_ptr = controller.get();
    controller_ptr->OnAccepted(
        base::DoNothingWithBoundArgs(std::move(controller)));
  } else if (button_index.value() == 1) {
    controller->OnDismissed();
  } else {
    NOTREACHED();
  }

  SystemNotificationHelper::GetInstance()->Close(kNotificationId);
}

void DefaultBrowserNotificationHandler::OnNotificationClose(bool by_user) {
  if (auto controller = std::exchange(controller_, nullptr)) {
    // If the notification closes without a click action (e.g. user clicked X
    // or system timeout), treat as dismissed.
    if (by_user) {
      controller->OnDismissed();
    } else {
      controller->OnIgnored();
    }
  }
}

}  // namespace default_browser
