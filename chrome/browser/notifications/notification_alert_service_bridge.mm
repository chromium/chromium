// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/notifications/notification_alert_service_bridge.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_operation.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

class MacNotificationActionHandlerImpl
    : public mac_notifications::mojom::MacNotificationActionHandler {
 public:
  explicit MacNotificationActionHandlerImpl(base::RepeatingClosure on_action)
      : on_action_(std::move(on_action)) {}
  ~MacNotificationActionHandlerImpl() override = default;

  // mac_notifications::mojom::MacNotificationActionHandler:
  void OnNotificationAction(
      mac_notifications::mojom::NotificationActionInfoPtr info) override {
    NSDictionary* dict = @{
      notification_constants::
      kNotificationId : base::SysUTF8ToNSString(info->meta->id->id),
      notification_constants::kNotificationProfileId :
          base::SysUTF8ToNSString(info->meta->id->profile->id),
      notification_constants::kNotificationIncognito :
          [NSNumber numberWithBool:info->meta->id->profile->incognito],
      notification_constants::kNotificationOrigin :
          base::SysUTF8ToNSString(info->meta->origin_url.spec()),
      notification_constants::kNotificationType : @(info->meta->type),
      notification_constants::
      kNotificationCreatorPid : @(info->meta->creator_pid),
      notification_constants::kNotificationOperation :
          [NSNumber numberWithInt:static_cast<int>(info->operation)],
      notification_constants::kNotificationButtonIndex : @(info->button_index),
      notification_constants::kNotificationIsAlert : @YES,
    };
    ProcessMacNotificationResponse(dict);
    on_action_.Run();
  }

 private:
  base::RepeatingCallback<void()> on_action_;
};

void DispatchGetNotificationsReply(
    base::mac::ScopedBlock<void (^)(NSArray*)> reply,
    std::vector<mac_notifications::mojom::NotificationIdentifierPtr>
        notifications) {
  NSMutableArray* alert_ids =
      [NSMutableArray arrayWithCapacity:notifications.size()];

  for (const auto& notification : notifications)
    [alert_ids addObject:base::SysUTF8ToNSString(notification->id)];

  reply.get()(alert_ids);
}

void DispatchGetAllNotificationsReply(
    base::mac::ScopedBlock<void (^)(NSArray*)> reply,
    std::vector<mac_notifications::mojom::NotificationIdentifierPtr>
        notifications) {
  NSMutableArray* alert_ids =
      [NSMutableArray arrayWithCapacity:notifications.size()];

  for (const auto& notification : notifications) {
    NSString* notification_id = base::SysUTF8ToNSString(notification->id);
    NSString* profile_id = base::SysUTF8ToNSString(notification->profile->id);
    NSNumber* incognito =
        [NSNumber numberWithBool:notification->profile->incognito];

    [alert_ids addObject:@{
      notification_constants::kNotificationId : notification_id,
      notification_constants::kNotificationProfileId : profile_id,
      notification_constants::kNotificationIncognito : incognito,
    }];
  }

  reply.get()(alert_ids);
}

}  // namespace

@implementation NotificationAlertServiceBridge {
  mojo::Remote<mac_notifications::mojom::MacNotificationProvider> _provider;
  mojo::Remote<mac_notifications::mojom::MacNotificationService> _service;
}

- (instancetype)
    initWithDisconnectHandler:(base::OnceClosure)onDisconnect
                actionHandler:(base::RepeatingClosure)onAction
                     provider:
                         (mojo::Remote<
                             mac_notifications::mojom::MacNotificationProvider>)
                             provider {
  if ((self = [super init])) {
    mojo::PendingRemote<mac_notifications::mojom::MacNotificationActionHandler>
        handlerRemote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<MacNotificationActionHandlerImpl>(std::move(onAction)),
        handlerRemote.InitWithNewPipeAndPassReceiver());

    _provider = std::move(provider);
    _provider.set_disconnect_handler(std::move(onDisconnect));
    _provider->BindNotificationService(_service.BindNewPipeAndPassReceiver(),
                                       std::move(handlerRemote));
  }
  return self;
}

- (void)setUseUNNotification:(BOOL)useUNNotification
           machExceptionPort:(CrXPCMachPort*)port {
  NOTREACHED();
}

- (void)deliverNotification:(NSDictionary*)notificationData {
  NSString* notificationId =
      [notificationData objectForKey:notification_constants::kNotificationId];
  NSString* profileId = [notificationData
      objectForKey:notification_constants::kNotificationProfileId];
  bool incognito = [[notificationData
      objectForKey:notification_constants::kNotificationIncognito] boolValue];

  auto profileIdentifier = mac_notifications::mojom::ProfileIdentifier::New(
      base::SysNSStringToUTF8(profileId), incognito);
  auto notificationIdentifier =
      mac_notifications::mojom::NotificationIdentifier::New(
          base::SysNSStringToUTF8(notificationId),
          std::move(profileIdentifier));

  int type = [[notificationData
      objectForKey:notification_constants::kNotificationType] intValue];
  GURL originUrl;
  if ([notificationData
          objectForKey:notification_constants::kNotificationOrigin]) {
    originUrl = GURL(base::SysNSStringToUTF8([notificationData
        objectForKey:notification_constants::kNotificationOrigin]));
  }
  int creatorPid = [[notificationData
      objectForKey:notification_constants::kNotificationCreatorPid] intValue];

  auto meta = mac_notifications::mojom::NotificationMetadata::New(
      std::move(notificationIdentifier), type, std::move(originUrl),
      creatorPid);

  std::u16string title = base::SysNSStringToUTF16([notificationData
      objectForKey:notification_constants::kNotificationTitle]);
  std::u16string subtitle = base::SysNSStringToUTF16([notificationData
      objectForKey:notification_constants::kNotificationSubTitle]);
  std::u16string body = base::SysNSStringToUTF16([notificationData
      objectForKey:notification_constants::kNotificationInformativeText]);
  bool renotify = [[notificationData
      objectForKey:notification_constants::kNotificationRenotify] boolValue];
  bool showSettingsButton = [[notificationData
      objectForKey:notification_constants::kNotificationHasSettingsButton]
      boolValue];

  std::vector<mac_notifications::mojom::NotificationActionButtonPtr> buttons;
  if ([notificationData
          objectForKey:notification_constants::kNotificationButtonOne]) {
    std::u16string title = base::SysNSStringToUTF16([notificationData
        objectForKey:notification_constants::kNotificationButtonOne]);
    auto button = mac_notifications::mojom::NotificationActionButton::New(
        std::move(title), /*placeholder=*/base::nullopt);
    buttons.push_back(std::move(button));
  }
  if ([notificationData
          objectForKey:notification_constants::kNotificationButtonTwo]) {
    std::u16string title = base::SysNSStringToUTF16([notificationData
        objectForKey:notification_constants::kNotificationButtonTwo]);
    auto button = mac_notifications::mojom::NotificationActionButton::New(
        std::move(title), /*placeholder=*/base::nullopt);
    buttons.push_back(std::move(button));
  }

  gfx::ImageSkia icon;
  if ([notificationData
          objectForKey:notification_constants::kNotificationIcon]) {
    NSImage* image = [notificationData
        objectForKey:notification_constants::kNotificationIcon];
    icon = gfx::Image(image).AsImageSkia();
  }

  auto notification = mac_notifications::mojom::Notification::New(
      std::move(meta), std::move(title), std::move(subtitle), std::move(body),
      renotify, showSettingsButton, std::move(buttons), std::move(icon));
  _service->DisplayNotification(std::move(notification));
}

- (void)closeNotificationWithId:(NSString*)notificationId
                      profileId:(NSString*)profileId
                      incognito:(BOOL)incognito {
  auto profileIdentifier = mac_notifications::mojom::ProfileIdentifier::New(
      base::SysNSStringToUTF8(profileId), incognito);
  auto notificationIdentifier =
      mac_notifications::mojom::NotificationIdentifier::New(
          base::SysNSStringToUTF8(notificationId),
          std::move(profileIdentifier));
  _service->CloseNotification(std::move(notificationIdentifier));
}

- (void)closeNotificationsWithProfileId:(NSString*)profileId
                              incognito:(BOOL)incognito {
  auto profileIdentifier = mac_notifications::mojom::ProfileIdentifier::New(
      base::SysNSStringToUTF8(profileId), incognito);
  _service->CloseNotificationsForProfile(std::move(profileIdentifier));
}

- (void)closeAllNotifications {
  _service->CloseAllNotifications();
}

- (void)getDisplayedAlertsForProfileId:(NSString*)profileId
                             incognito:(BOOL)incognito
                                 reply:(void (^)(NSArray*))reply {
  auto profileIdentifier = mac_notifications::mojom::ProfileIdentifier::New(
      base::SysNSStringToUTF8(profileId), incognito);
  _service->GetDisplayedNotifications(
      std::move(profileIdentifier),
      base::BindOnce(&DispatchGetNotificationsReply, base::RetainBlock(reply)));
}

- (void)getAllDisplayedAlertsWithReply:(void (^)(NSArray*))reply {
  _service->GetDisplayedNotifications(
      /*profileIdentifier=*/nullptr,
      base::BindOnce(&DispatchGetAllNotificationsReply,
                     base::RetainBlock(reply)));
}

@end
