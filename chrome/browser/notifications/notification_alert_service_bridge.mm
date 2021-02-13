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
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

class MacNotificationActionHandlerImpl
    : public notifications::mojom::MacNotificationActionHandler {
 public:
  // notifications::mojom::MacNotificationActionHandler:
  void OnNotificationAction(
      notifications::mojom::NotificationActionInfoPtr info) override {
    NSDictionary* dict = nil;
    // TODO(knollr): Get all properties from |info| into |dict|.
    ProcessMacNotificationResponse(dict);
  }

  mojo::PendingRemote<notifications::mojom::MacNotificationActionHandler>
  BindRemote() {
    return binding_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<notifications::mojom::MacNotificationActionHandler> binding_{
      this};
};

void DispatchGetNotificationsReply(
    base::mac::ScopedBlock<void (^)(NSArray*)> reply,
    std::vector<notifications::mojom::NotificationIdentifierPtr>
        notifications) {
  NSMutableArray* alert_ids =
      [NSMutableArray arrayWithCapacity:notifications.size()];

  for (const auto& notification : notifications)
    [alert_ids addObject:base::SysUTF8ToNSString(notification->id)];

  reply.get()(alert_ids);
}

void DispatchGetAllNotificationsReply(
    base::mac::ScopedBlock<void (^)(NSArray*)> reply,
    std::vector<notifications::mojom::NotificationIdentifierPtr>
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
  mojo::Remote<notifications::mojom::MacNotificationProvider> _provider;
  mojo::Remote<notifications::mojom::MacNotificationService> _service;
  MacNotificationActionHandlerImpl _handler;
}

- (instancetype)initWithDisconnectHandler:(base::OnceClosure)onDisconect {
  if ((self = [super init])) {
    // Launches a new helper process and sets up a mojo connection to it. If the
    // mojo connection disconnects we call |onDisconnect| which will destroy
    // |this| and terminate the process if it hasn't been already.
    _provider = content::ServiceProcessHost::Launch<
        notifications::mojom::MacNotificationProvider>(
        content::ServiceProcessHost::Options()
            .WithExtraCommandLineSwitches({switches::kMessageLoopTypeUi})
            // TODO(knollr): Set the correct flags so the helper launches via
            // the app which has set the alert notifications style:
            //.WithChildFlags(chrome::kChildProcessHelperAlerts)
            .Pass());
    _provider.set_disconnect_handler(std::move(onDisconect));
    _provider->BindNotificationService(_service.BindNewPipeAndPassReceiver(),
                                       _handler.BindRemote());
  }
  return self;
}

- (instancetype)
    initWithDisconnectHandler:(base::OnceClosure)onDisconect
                     provider:
                         (mojo::PendingRemote<
                             notifications::mojom::MacNotificationProvider>)
                             provider {
  if ((self = [super init])) {
    _provider.Bind(std::move(provider));
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
  // TODO(knollr): implement.
}

- (void)closeNotificationWithId:(NSString*)notificationId
                      profileId:(NSString*)profileId
                      incognito:(BOOL)incognito {
  auto profileIdentifier = notifications::mojom::ProfileIdentifier::New(
      base::SysNSStringToUTF8(profileId), incognito);
  auto notificationIdentifier =
      notifications::mojom::NotificationIdentifier::New(
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
  auto profileIdentifier = notifications::mojom::ProfileIdentifier::New(
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
