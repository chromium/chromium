// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/error_notification/arc_error_notification_bridge.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/vector_icons/vector_icons.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/error_notification/arc_error_notification_item.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace arc {

namespace {

// Singleton factory for ArcErrorNotificationBridgeFactory.
class ArcErrorNotificationBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcErrorNotificationBridge,
          ArcErrorNotificationBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcErrorNotificationBridgeFactory";

  static ArcErrorNotificationBridgeFactory* GetInstance() {
    return base::Singleton<ArcErrorNotificationBridgeFactory>::get();
  }

  ArcErrorNotificationBridgeFactory() = default;
  ~ArcErrorNotificationBridgeFactory() override = default;
};

}  // namespace

// static
ArcErrorNotificationBridge* ArcErrorNotificationBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcErrorNotificationBridgeFactory::GetForBrowserContext(context);
}

// static
void ArcErrorNotificationBridge::EnsureFactoryBuilt() {
  ArcErrorNotificationBridgeFactory::GetInstance();
}

ArcErrorNotificationBridge::ArcErrorNotificationBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      profile_(Profile::FromBrowserContext(context)) {
  arc_bridge_service_->error_notification()->SetHost(this);
}

ArcErrorNotificationBridge::~ArcErrorNotificationBridge() {
  arc_bridge_service_->error_notification()->SetHost(nullptr);
}

class ErrorNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit ErrorNotificationDelegate(
      base::WeakPtr<ArcErrorNotificationBridge> bridge,
      mojo::PendingRemote<mojom::ErrorNotificationActionHandler> action_handler,
      std::string notification_id)
      : bridge_(bridge),
        action_handler_(std::move(action_handler)),
        notification_id_(notification_id) {}

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    if (button_index && bridge_ && action_handler_) {
      action_handler_->OnNotificationButtonClicked(
          static_cast<uint32_t>(button_index.value()));
      bridge_->CloseNotification(notification_id_);
    }
  }

  void Close(bool by_user) override {
    if (bridge_ && action_handler_) {
      action_handler_->OnNotificationClosed();
    }
  }

 protected:
  ~ErrorNotificationDelegate() override = default;

 private:
  base::WeakPtr<ArcErrorNotificationBridge> bridge_;
  mojo::Remote<mojom::ErrorNotificationActionHandler> action_handler_;
  std::string notification_id_;
};

void ArcErrorNotificationBridge::SendErrorDetails(
    mojom::ErrorDetailsPtr details,
    mojo::PendingRemote<mojom::ErrorNotificationActionHandler> action_handler,
    SendErrorDetailsCallback callback) {
  if (!base::FeatureList::IsEnabled(arc::kEnableFriendlierErrorDialog)) {
    LOG(WARNING) << "ARC is currently sending Chrome Error details, but the "
                    "Friendlier Error Dialog feature is not activated.";
    return;
  }

  const std::string& name = details->name;
  const std::string notification_id =
      GenerateNotificationId(details->type, name);

  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE /* type */, notification_id,
      base::UTF8ToUTF16(details->title.c_str()), u"" /* message */,
      base::UTF8ToUTF16(name.c_str()) /* display_source */,
      GURL() /* origin_url */, message_center::NotifierId(),
      message_center::RichNotificationData() /* optional_fields */,
      base::MakeRefCounted<ErrorNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr(), std::move(action_handler),
          notification_id),
      kErrorNotificationIcon,
      message_center::SystemNotificationWarningLevel::WARNING);

  std::vector<message_center::ButtonInfo> buttons;
  for (const std::string& label : *details->buttonLabels) {
    buttons.push_back(message_center::ButtonInfo(base::UTF8ToUTF16(label)));
  }
  notification.set_buttons(buttons);

  notification.set_never_timeout(true);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      nullptr /* metadata */);

  std::move(callback).Run(ArcErrorNotificationItem::Create(
      weak_ptr_factory_.GetWeakPtr(), notification_id));
}

void ArcErrorNotificationBridge::CloseNotification(
    const std::string& notification_id) {
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, notification_id);
}

std::string ArcErrorNotificationBridge::GenerateNotificationId(
    mojom::ErrorType type,
    const std::string& name) {
  switch (type) {
    case mojom::ErrorType::ANR:
      return "ANR_" + name;
    case mojom::ErrorType::CRASH:
      return "CRASH_" + name;
  }
}

}  // namespace arc
