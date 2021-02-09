// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_ALERT_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_ALERT_SERVICE_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/callback_forward.h"
#import "chrome/browser/ui/cocoa/notifications/notification_delivery.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

// Class that implements the NotificationDelivery protocol and forwards it to
// the mojo service-backed implementation. The lifetime of this class is tied to
// the lifetime of the utility process connected via mojo.
@interface NotificationAlertServiceBridge : NSObject <NotificationDelivery>

- (instancetype)initWithDisconnectHandler:(base::OnceClosure)onDisconect;

// This init method is available to pass a custom |provider| in tests.
- (instancetype)
    initWithDisconnectHandler:(base::OnceClosure)onDisconect
                     provider:
                         (mojo::PendingRemote<
                             notifications::mojom::MacNotificationProvider>)
                             provider;

@end

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_ALERT_SERVICE_BRIDGE_H_
