// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_MOJO_H_
#define CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_MOJO_H_

#import <Foundation/Foundation.h>

#include <memory>

#import "chrome/browser/notifications/alert_dispatcher_mac.h"
#include "chrome/browser/notifications/mac_notification_provider_factory.h"

// Implementation of the AlertDispatcher interface to display notifications via
// a Mojo Service running in a helper process.
@interface AlertDispatcherMojo : NSObject <AlertDispatcher>

- (instancetype)initWithProviderFactory:
    (std::unique_ptr<MacNotificationProviderFactory>)providerFactory;

@end

#endif  // CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_MOJO_H_
