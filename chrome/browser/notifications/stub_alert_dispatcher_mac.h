// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_STUB_ALERT_DISPATCHER_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_STUB_ALERT_DISPATCHER_MAC_H_

#import <Foundation/Foundation.h>

#include "chrome/browser/notifications/alert_dispatcher_mac.h"

@interface StubAlertDispatcher : NSObject<AlertDispatcher>
- (NSArray*)alerts;
@end

#endif  // CHROME_BROWSER_NOTIFICATIONS_STUB_ALERT_DISPATCHER_MAC_H_
