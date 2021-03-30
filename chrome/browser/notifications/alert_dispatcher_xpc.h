// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_XPC_H_
#define CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_XPC_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/notifications/alert_dispatcher_mac.h"

// Implementation of the AlertDispatcher interface to display notifications via
// an XPC Service.
// TODO(crbug.com/1170731): Remove this in favor of a helper app.
@interface AlertDispatcherXPC : NSObject <AlertDispatcher>
@end

#endif  // CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_XPC_H_
