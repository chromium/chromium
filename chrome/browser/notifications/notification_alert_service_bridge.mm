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
#include "chrome/browser/service_sandbox_type.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_operation.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace {

class MacNotificationActionHandlerImpl
    : public mac_notifications::mojom::MacNotificationActionHandler {
 public:
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
    };
    ProcessMacNotificationResponse(dict);
  }

  mojo::PendingRemote<mac_notifications::mojom::MacNotificationActionHandler>
  BindRemote() {
    return binding_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mac_notifications::mojom::MacNotificationActionHandler>
      binding_{this};
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
  MacNotificationActionHandlerImpl _handler;
}

- (instancetype)initWithDisconnectHandler:(base::OnceClosure)onDisconect {
  return [self initWithDisconnectHandler:std::move(onDisconect)
                                provider:mojo::NullRemote()];
}

- (instancetype)
    initWithDisconnectHandler:(base::OnceClosure)onDisconect
                     provider:
                         (mojo::PendingRemote<
                             mac_notifications::mojom::MacNotificationProvider>)
                             provider {
  if ((self = [super init])) {
    if (provider) {
      // Use the passed in |provider| to setup the mojo connection. This is used
      // in tests so we don't have to spin up a new process and can pass in a
      // mocked service.
      _provider.Bind(std::move(provider));
    } else {
      // Launches a new helper process and sets up a mojo connection to it. If
      // the mojo connection disconnects we call |onDisconnect| which will
      // destroy |this| and terminate the process if it hasn't been already.
      _provider = content::ServiceProcessHost::Launch<
          mac_notifications::mojom::MacNotificationProvider>(
          content::ServiceProcessHost::Options()
              .WithExtraCommandLineSwitches({switches::kMessageLoopTypeUi})
              // TODO(knollr): Set the correct flags so the helper launches via
              // the app which has set the alert notifications style:
              //.WithChildFlags(chrome::kChildProcessHelperAlerts)
              .Pass());
    }

    _provider.set_disconnect_handler(std::move(onDisconect));
    _provider->BindNotificationService(_service.BindNewPipeAndPassReceiver(),
                                       _handler.BindRemote());
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

  // TODO(knollr): Pass properties from |notificationData| into
  // |notification|.
  auto notification =
      mac_notifications::mojom::Notification::New(std::move(meta));

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
